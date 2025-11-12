
# Final IoT Project

Resumen
-------
Proyecto de control robótico e IoT que integra múltiples sensores y comunicación LoRa. El sistema utiliza cuatro módulos LoRa: un par para control de motores (a través de un puente H) y otro par para transmisión de datos de sensores. Las lecturas de sensores se almacenan en Orion (FIWARE) y se visualizan con Grafana para análisis.

Índice
------
- [Resumen](#resumen)
- [Características](#características)
- [Arquitectura e infraestructura](#arquitectura-e-infraestructura)
- [Requisitos](#requisitos)
- [Estructura de carpetas](#estructura-de-carpetas)
- [Cómo usar / Despliegue rápido](#cómo-usar--despliegue-rápido)
  - [Compilar y cargar el firmware](#compilar-y-cargar-el-firmware)
  - [Levantar la infraestructura (ejemplo con Docker Compose)](#levantar-la-infraestructura-ejemplo-con-docker-compose)
  - [Configurar Grafana para visualizar datos](#configurar-grafana-para-visualizar-datos)
- [Detalles de hardware](#detalles-de-hardware)
- [Flujo de datos](#flujo-de-datos)
- [Contribuir](#contribuir)
- [Contacto y licencias](#contacto-y-licencias)

Características
---------------
- Control remoto de motores vía LoRa y puente H (H-bridge).
- Recolección inalámbrica de datos de sensores mediante LoRa.
- Almacenamiento de contexto y telemetría en Orion (FIWARE).
- Visualización y análisis con Grafana.
- Código principal en C++ (firmware/embedded).

Arquitectura e infraestructura
------------------------------
Visión general:
- Nodos sensores (microcontroladores con LoRa) recolectan datos del entorno.
- Un nodo controlador (o par de nodos) gestiona el control de motores mediante H-bridge usando LoRa.
- Un gateway/puente (p. ej. Raspberry Pi u otro concentrador) recibe paquetes LoRa y los reenvía a la infraestructura backend.
- Backend FIWARE: Orion Context Broker almacena el estado/contexto de sensores.
- Canal de series temporales (opcional): QuantumLeap / Cygnus para transformar las actualizaciones de Orion a una base de datos de series temporales (InfluxDB, CrateDB, etc.).
- Grafana se conecta a la base de datos de series temporales para dashboards y análisis.

Componentes típicos (implementación habitual):
- Hardware: microcontroladores (ESP32/Arduino/STM32), módulos LoRa (SX127x), H-bridge (L298N, TB6612, DRV8833, según potencia).
- Software embebido: C++ (PlatformIO / Arduino).
- Infraestructura: Docker Compose con Orion (FIWARE), MongoDB (persistencia de Orion), QuantumLeap (opcional para series temporales), InfluxDB, Grafana.

Requisitos
---------
- Herramientas de compilación para C++ embebido (p. ej. PlatformIO o Arduino IDE).
- Librerías LoRa (p. ej. RadioHead, LoRaLib o LoRa de Sandeep Mistry según plataforma).
- Docker y Docker Compose (si se usa el despliegue por contenedores).
- Acceso a hardware: módulos LoRa, microcontroladores, H-bridge y sensores.

Cómo usar / Despliegue rápido
-----------------------------

1) Compilar y cargar el firmware
- Recomendado: PlatformIO (más reproducible) o Arduino IDE.
- Ejemplo con PlatformIO:
  - Instalar PlatformIO.
  - Entrar al subdirectorio del firmware:
    - cd firmware
  - Compilar:
    - pio run
  - Subir al dispositivo (ajusta el environment a tu placa):
    - pio run -t upload -e <tu_env>

- Variables típicas a configurar en el firmware:
  - Parámetros LoRa: frecuencia, spreading factor, power, SS/CS pin.
  - Pines del H-bridge y mapeo de motores.
  - Endpoint del gateway/backend (URL y puerto de la API que recibe datos).

2) Levantar la infraestructura (ejemplo con Docker Compose)
- Ejemplo de docker-compose (sólo es un esqueleto de ejemplo; adaptar versiones y variables):

```yaml
version: "3.7"
services:
  mongo:
    image: mongo:4.4
    container_name: fiware_mongo
    restart: unless-stopped
    volumes:
      - mongo-db:/data/db
    ports:
      - "27017:27017"

  orion:
    image: fiware/orion
    container_name: fiware_orion
    environment:
      - MONGO_HOST=mongo
    depends_on:
      - mongo
    ports:
      - "1026:1026"

  quantumleap:
    image: smartsdk/quantumleap
    container_name: quantumleap
    depends_on:
      - orion
      - influxdb
    environment:
      - CRATE_HOST=crate
    ports:
      - "8668:8668"

  influxdb:
    image: influxdb:1.8
    container_name: influxdb
    ports:
      - "8086:8086"
    volumes:
      - influxdb-db:/var/lib/influxdb

  grafana:
    image: grafana/grafana:latest
    container_name: grafana
    environment:
      - GF_SECURITY_ADMIN_PASSWORD=admin
    ports:
      - "3000:3000"
    depends_on:
      - influxdb

volumes:
  mongo-db:
  influxdb-db:
```

- Levantar:
  - docker-compose -f infra/docker-compose.yml up -d

- Notas:
  - QuantumLeap (u otro componente) sirve para transformar las actualizaciones de Orion a una base de datos de series temporales leíble por Grafana (InfluxDB en el ejemplo).
  - Ajusta imágenes y versiones según la compatibilidad deseada.

3) Configurar Grafana
- Accede a Grafana: http://localhost:3000 (usuario/admin por defecto).
- Añadir Datasource: InfluxDB (o la base que hayas elegido).
- Crear dashboards importando métricas enviadas desde QuantumLeap/InfluxDB o directamente desde la fuente de datos que uses.

Detalles de hardware
--------------------
- Módulos LoRa: SX1276/SX1278 u otros compatibles (frecuencia: 433/868/915 MHz según región).
- Microcontroladores: ESP32/Arduino/STM32 (ajustar entornos y pines).
- H-bridge: L298N / TB6612 / DRV8833 (elige según corriente requerida por los motores).
- Sensores: colocar una lista de sensores usados (ej.: ultrasonido HC-SR04, acelerómetro/giroscopio MPU6050, encoders rotatorios, sensores ambientales), con sus conexiones y requisitos de alimentación.

Flujo de datos (resumido)
-------------------------
1. Sensores -> microcontrolador (lectura periódica).
2. Microcontrolador -> paquete LoRa -> receptor/gateway.
3. Gateway -> publica datos al Orion Context Broker (HTTP).
4. Orion almacena el contexto (y persiste en MongoDB).
5. QuantumLeap/Cygnus (u otro) suscrito a Orion convierte y guarda series temporales en InfluxDB/CrateDB.
6. Grafana lee series temporales y muestra dashboards.

Consejos de configuración y seguridad
-------------------------------------
- Configurar correctamente las frecuencias LoRa según normativa local.
- Habilitar autenticación y TLS en los servicios de backend para entornos de producción (Orion, Grafana).
- Asegurar el acceso al gateway (SSH y firewall).
- Controlar el consumo energético de los nodos si es necesario (deep-sleep, duty cycle).
