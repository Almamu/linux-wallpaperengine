terraform {
  required_version = ">= 1.5"

  required_providers {
    github = {
      source  = "integrations/github"
      version = "~> 6.0"
    }
  }
}

# Reads the PAT from the GITHUB_TOKEN env var automatically.
provider "github" {
  owner = var.github_owner
}
