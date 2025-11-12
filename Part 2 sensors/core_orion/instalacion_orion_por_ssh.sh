#!/bin/bash
set -euo pipefail

llave="$1"
user="$2"
ip="$3"

if [[ -z "${llave}" || -z "${user}" || -z "${ip}" ]]; then
  echo "Uso: $0 /ruta/a/llave.pem usuario@ o usuario ip"
  echo "Ejemplo: $0 ~/.ssh/mi_llave.pem ubuntu 1.2.3.4"
  exit 1
fi

if [[ ! -f "${llave}" ]]; then
  echo "La llave '${llave}' no existe."
  exit 2
fi

if [[ ! -f "docker-compose.yml" ]]; then
  echo "No se encontró 'docker-compose.yml' en el directorio actual."
  exit 3
fi

echo "Enviando docker-compose.yml a ${user}@${ip} ..."
scp -i "${llave}" -o StrictHostKeyChecking=no "docker-compose.yml" "${user}@${ip}:~/"

echo "Ejecutando instalación remota en ${user}@${ip} ..."
ssh -i "${llave}" -o StrictHostKeyChecking=no "${user}@${ip}" bash -s <<'REMOTE'
set -euo pipefail

# Comprobación de privilegios (sudo disponible)
if ! command -v sudo >/dev/null 2>&1; then
  echo "sudo no está disponible en el remoto. Abortando."
  exit 10
fi

# Actualizar repositorios y paquetes (no interactivo)
sudo apt-get update
sudo DEBIAN_FRONTEND=noninteractive apt-get -y upgrade

# Instalar dependencias básicas
sudo apt-get -y install ca-certificates curl gnupg lsb-release

# Crear keyrings dir
sudo install -m 0755 -d /etc/apt/keyrings

# Añadir la clave GPG oficial de Docker
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg
sudo chmod a+r /etc/apt/keyrings/docker.gpg

# Añadir repositorio Docker (para Ubuntu)
echo \
  "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/ubuntu \
  $(. /etc/os-release && echo "${UBUNTU_CODENAME:-$VERSION_CODENAME}") stable" | \
  sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

sudo apt-get update

# Instalar Docker CE y plugins
sudo apt-get -y install docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin

# Iniciar/enable docker si es necesario
sudo systemctl enable --now docker

# Probar Docker
sudo docker run --rm hello-world || true

# Ir al home y lanzar docker compose
cd ~
# Intentar usar el plugin integrado primero; si no existe, usar docker-compose
if sudo docker compose version >/dev/null 2>&1; then
  sudo docker compose up -d
elif command -v docker-compose >/dev/null 2>&1; then
  sudo docker-compose up -d
else
  echo "No se encontró 'docker compose' ni 'docker-compose'."
  exit 20
fi

REMOTE

echo "Hecho. Revisa la salida anterior para errores."