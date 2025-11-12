# Terraform: WS Bridge + Web Page (Single EC2)

Despliega una única instancia EC2 con:
- Docker instalado por `user_data`.
- Contenedor FastAPI (puente WebSocket) exponiendo puerto 8000.
- Contenedor Nginx sirviendo la página estática en puerto 80.

## Recursos
- `aws_instance.bridge`: Amazon Linux 2023.
- `aws_security_group.bridge`: Puertos abiertos 80 (HTTP) y 8000 (WebSocket/control API).
- AMI seleccionada dinámicamente (AL2023 x86_64 más reciente).

## Variables Principales
| Nombre | Descripción | Default |
| ------ | ----------- | ------- |
| region | Región AWS | us-east-1 |
| name | Prefijo de nombre/etiquetas | ws-bridge |
| allowed_cidrs | Lista CIDRs permitidos | ["0.0.0.0/0"] |
| instance_type | Tipo de instancia EC2 | t3.micro |
| key_name | Par de llaves existente para SSH (KeyPair) | orion |
| vpc_id | VPC ID opcional | null |
| subnet_id | Subnet ID opcional | null |
| tags | Tags adicionales | {} |

Asegúrate de que el KeyPair `orion` existe en la región antes del `apply`.

## Uso

1. Crear/editar `terraform.tfvars` si quieres override de variables, por ejemplo:
   ```hcl
   region = "us-east-1"
   key_name = "orion"
   allowed_cidrs = ["203.0.113.0/24"]
   instance_type = "t3.small"
   tags = { env = "dev" }
   ```
2. Inicializar y aplicar:
   ```bash
   terraform init
   terraform apply -auto-approve
   ```
3. Salidas:
   - `public_ip`
   - `public_dns`

4. Conexiones:
   - Página web: `http://<public_ip>/` (puerto 80)
   - WebSocket control: `ws://<public_ip>:8000/ws/control`
   - ESP32 debe conectarse a: `ws://<public_ip>:8000/ws/tank/<tankId>`

## Flujo Operativo
- Al boot, `user_data` instala Docker, construye la imagen del puente y levanta dos contenedores.
- La página estática se sirve vía Nginx (contenedor). El puente sirve WebSocket y endpoints `/health`, etc.

## Extensiones Futuras
- Añadir TLS (ALB o Nginx con certbot) para `https://` y `wss://`.
- Mover a ECS/Fargate si se escala.
- Añadir CloudWatch Logs mediante agente.
- Añadir parámetro para restringir CIDRs.

## Limpieza
```bash
terraform destroy -auto-approve
```

## Seguridad
- Actualmente permite acceso público (`0.0.0.0/0`). Ajusta `allowed_cidrs` para restringir.
- Añade autenticación para comandos si se requiere (tokens JWT en WS).

