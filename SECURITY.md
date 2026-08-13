# Security Policy

This project is an IoT prototype. Do not commit real Wi-Fi credentials, MQTT credentials, API keys, certificates, tokens or production endpoints.

Use `include/secrets.example.h` as the template for local configuration and keep the real `include/secrets.h` file outside version control.

If a credential is ever committed, rotate it immediately. Removing it from the latest branch is not sufficient because it may still exist in Git history.

For production use, enable certificate verification, use unique device credentials, apply least-privilege broker permissions, protect firmware update paths and monitor authentication failures.

Security issues should be reported privately to the repository owner rather than posted publicly with active credentials or exploit details.
