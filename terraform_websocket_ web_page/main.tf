terraform {
  required_version = ">= 1.6.0"

  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.0"
    }
  }
}

provider "aws" {
  region = var.region
}

data "aws_vpc" "default" {
  count   = var.vpc_id == null ? 1 : 0
  default = true
}

locals {
  vpc_id = var.vpc_id != null ? var.vpc_id : data.aws_vpc.default[0].id
}

data "aws_subnets" "selected" {
  count = var.subnet_id == null ? 1 : 0
  filter {
    name   = "vpc-id"
    values = [local.vpc_id]
  }
}

locals {
  subnet_id = var.subnet_id != null ? var.subnet_id : data.aws_subnets.selected[0].ids[0]
  merged_tags = merge({
    Name = var.name
  }, var.tags)

  # Keep user_data small; it will curl raw files from GitHub on boot
  user_data = templatefile("${path.module}/templates/user_data.sh.tpl", {})
}

resource "aws_security_group" "bridge" {
  name_prefix = "${var.name}-sg-"
  description = "Rules for WS bridge and static web"
  vpc_id      = local.vpc_id

  # HTTP for web page
  ingress {
    description = "HTTP"
    from_port   = 80
    to_port     = 80
    protocol    = "tcp"
    cidr_blocks = var.allowed_cidrs
  }

  # WS bridge (FastAPI)
  ingress {
    description = "WS bridge"
    from_port   = 8000
    to_port     = 8000
    protocol    = "tcp"
    cidr_blocks = var.allowed_cidrs
  }

  egress {
    from_port        = 0
    to_port          = 0
    protocol         = "-1"
    cidr_blocks      = ["0.0.0.0/0"]
    ipv6_cidr_blocks = ["::/0"]
  }

  tags = local.merged_tags
}

data "aws_ami" "al2023" {
  count       = var.ami_id == null ? 1 : 0
  most_recent = true
  owners      = ["amazon"]

  filter {
    name   = "name"
    values = ["al2023-ami-*-x86_64"]
  }

  filter {
    name   = "architecture"
    values = ["x86_64"]
  }
}

resource "aws_instance" "bridge" {
  ami                         = var.ami_id != null ? var.ami_id : data.aws_ami.al2023[0].id
  instance_type               = var.instance_type
  subnet_id                   = local.subnet_id
  associate_public_ip_address = true
  key_name                    = var.key_name
  vpc_security_group_ids      = [aws_security_group.bridge.id]

  user_data = local.user_data

  tags = local.merged_tags
}

output "public_ip" {
  value       = aws_instance.bridge.public_ip
  description = "Public IP of the WS bridge EC2"
}

output "public_dns" {
  value       = aws_instance.bridge.public_dns
  description = "Public DNS of the WS bridge EC2"
}
