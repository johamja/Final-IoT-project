FROM python:3.11-slim
WORKDIR /app

# Prevent Python from buffering stdout/err
ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1

COPY requirements.txt /app/requirements.txt
RUN pip install --no-cache-dir -r /app/requirements.txt

RUN mkdir -p /app/bridge
COPY server.py /app/bridge/server.py

ENV PORT=8000
CMD ["python", "/app/bridge/server.py"]
