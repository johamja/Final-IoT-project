variable "region" {
  description = "AWS region to deploy the bridge"
  type        = string
  default     = "us-east-1"
}

variable "name" {
  description = "Base name for tags and resources"
  type        = string
  default     = "ws-bridge"
}

variable "allowed_cidrs" {
  description = "CIDR blocks allowed to access HTTP(80) and WS(8000)"
  type        = list(string)
  default     = ["0.0.0.0/0"]
}

variable "instance_type" {
  description = "EC2 instance type"
  type        = string
  default     = "t3.micro"
}

variable "key_name" {
  description = "Existing EC2 key pair to use for SSH"
  type        = string
  default     = "orion"
}

variable "vpc_id" {
  description = "Optional VPC ID (defaults to account's default VPC)"
  type        = string
  default     = null
}

variable "subnet_id" {
  description = "Optional subnet ID (defaults to first subnet in the VPC)"
  type        = string
  default     = null
}

variable "ami_id" {
  description = "Optional AMI ID to use directly (bypasses DescribeImages). If null, latest AL2023 x86_64 is used."
  type        = string
  default     = null
}

variable "tags" {
  description = "Additional tags for all resources"
  type        = map(string)
  default     = {}
}
