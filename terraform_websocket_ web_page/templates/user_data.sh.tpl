#!/bin/bash
set -euxo pipefail

# Capture all output from this script into cloud-init logs and our own file
exec > >(tee -a /var/log/cloud-init-output.log /var/log/user-data.log) 2>&1

echo "[userdata] Updating system and installing Docker"
# Amazon Linux 2023 uses dnf
until dnf -y update; do echo "[userdata] dnf update retry"; sleep 5; done
until dnf -y install docker curl; do echo "[userdata] dnf install retry"; sleep 5; done
systemctl enable --now docker
usermod -aG docker ec2-user || true

mkdir -p /opt/puente /opt/web /opt/bin
chmod -R 755 /opt/puente /opt/web /opt/bin

# Write bootstrap script to run after network and docker are ready
cat > /opt/bin/bootstrap.sh <<'EOF'
#!/bin/bash
set -euxo pipefail
LOG_FILE=/var/log/bridge-bootstrap.log
# Pre-create log file so its absence later indicates script not started
touch "$LOG_FILE"
echo "[bootstrap][stage=init] starting bootstrap script" >> "$LOG_FILE"
exec > >(tee -a "$LOG_FILE") 2>&1

echo "[bootstrap][stage=download] Downloading bridge files"
curl -fsSL "https://raw.githubusercontent.com/johamja/iot_web_controller/main/infra/terraform/bridge_puente/templates/requirements.txt" -o /opt/puente/requirements.txt
curl -fsSL "https://raw.githubusercontent.com/johamja/iot_web_controller/main/infra/terraform/bridge_puente/templates/server.py" -o /opt/puente/server.py
curl -fsSL "https://raw.githubusercontent.com/johamja/iot_web_controller/main/infra/terraform/bridge_puente/templates/Dockerfile" -o /opt/puente/Dockerfile
curl -fsSL "https://raw.githubusercontent.com/johamja/iot_web_controller/main/infra/terraform/bridge_puente/templates/index.html" -o /opt/web/index.html

echo "[bootstrap][stage=build] Building bridge image"
docker build -t tank-bridge:latest /opt/puente

echo "[bootstrap][stage=pull] Pulling nginx image"
docker pull nginx:alpine

echo "[bootstrap][stage=run] (Re)starting containers"
docker rm -f bridge || true
docker rm -f web || true

docker run -d --name bridge -p 8000:8000 --restart unless-stopped \
  tank-bridge:latest

docker run -d --name web -p 80:80 --restart unless-stopped \
  -v /opt/web:/usr/share/nginx/html:ro \
  nginx:alpine

touch /opt/bridge-bootstrap.done
echo "[bootstrap][stage=done] Bridge WS on tcp/8000, static web on tcp/80"
EOF

chmod +x /opt/bin/bootstrap.sh

# Create a systemd unit to execute bootstrap after network and docker are up
cat > /etc/systemd/system/bridge-bootstrap.service <<'EOF'
[Unit]
Description=Bridge Puente Bootstrap
Wants=network-online.target docker.service
After=network-online.target docker.service

[Service]
Type=oneshot
Environment=BRANCH=main
ExecStart=/opt/bin/bootstrap.sh
RemainAfterExit=yes
Restart=on-failure
RestartSec=10
StandardOutput=journal+console
StandardError=journal+console

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
systemctl enable bridge-bootstrap.service
systemctl start bridge-bootstrap.service

echo "[userdata] Bootstrap service launched; check 'journalctl -u bridge-bootstrap' for logs"
