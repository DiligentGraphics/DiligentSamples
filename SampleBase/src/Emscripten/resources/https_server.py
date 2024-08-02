import os
import socket
import signal
import sys
import tempfile
import subprocess
import argparse
import platform
from http.server import SimpleHTTPRequestHandler, HTTPServer

class CustomHandler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        super().end_headers()

def run_http(server_class=HTTPServer, handler_class=CustomHandler, port=80):
    server_address = ('', port)
    httpd = server_class(server_address, handler_class)
    
    hostname = socket.gethostname()
    ipv4_address = socket.gethostbyname(hostname)

    def signal_handler(signal, frame):
        print('\nKeyboard interrupt received, exiting.')
        httpd.server_close()
        sys.exit(0)

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    print(f'Serving HTTP on {ipv4_address} port {port} (http://{ipv4_address}:{port}/) ...')
    try:
        httpd.serve_forever()
    except Exception as e:
        print(f"Server error: {e}")
        httpd.server_close()

def run_https(server_class=HTTPServer, handler_class=CustomHandler, port=443, register_cert=False):
    try:
        from cryptography import x509
        from cryptography.x509.oid import NameOID
        from cryptography.hazmat.primitives import serialization, hashes
        from cryptography.hazmat.primitives.asymmetric import rsa
        from cryptography.hazmat.backends import default_backend
        from datetime import datetime, timedelta, timezone
        import ssl
    except ImportError:
        print("Error: The 'cryptography' module is required to run the server in HTTPS mode.")
        sys.exit(1)

    def get_system_info():
        country = os.environ.get('COUNTRY', 'US')
        state = os.environ.get('STATE', 'Unknown State')
        locality = os.environ.get('CITY', 'Unknown Locality')
        organization = os.environ.get('ORGANIZATION', platform.node())
        common_name = os.environ.get('COMMON_NAME', 'localhost')

        return country, state, locality, organization, common_name

    def generate_self_signed_cert():
        country, state, locality, organization, common_name = get_system_info()

        key = rsa.generate_private_key(
            public_exponent=65537,
            key_size=2048,
            backend=default_backend()
        )

        subject = issuer = x509.Name([
            x509.NameAttribute(NameOID.COUNTRY_NAME, country),
            x509.NameAttribute(NameOID.STATE_OR_PROVINCE_NAME, state),
            x509.NameAttribute(NameOID.LOCALITY_NAME, locality),
            x509.NameAttribute(NameOID.ORGANIZATION_NAME, organization),
            x509.NameAttribute(NameOID.COMMON_NAME, common_name),
        ])

        cert = x509.CertificateBuilder().subject_name(
            subject
        ).issuer_name(
            issuer
        ).public_key(
            key.public_key()
        ).serial_number(
            x509.random_serial_number()
        ).not_valid_before(
            datetime.now(timezone.utc)
        ).not_valid_after(
            datetime.now(timezone.utc) + timedelta(days=1)
        ).add_extension(
            x509.SubjectAlternativeName([x509.DNSName(common_name)]),
            critical=False,
        ).sign(key, hashes.SHA256(), default_backend())

        key_pem = key.private_bytes(
            encoding=serialization.Encoding.PEM,
            format=serialization.PrivateFormat.TraditionalOpenSSL,
            encryption_algorithm=serialization.NoEncryption()
        )

        cert_pem = cert.public_bytes(serialization.Encoding.PEM)

        return key_pem, cert_pem

    def trust_certificate(cert_pem, cert_filename):
        if os.name == 'nt':
            subprocess.run(['certutil', '-addstore', 'Root', cert_filename], check=True)
        elif sys.platform == 'darwin':
            subprocess.run(['sudo', 'security', 'add-trusted-cert', '-d', '-r', 'trustRoot', '-k',
                            '/Library/Keychains/System.keychain', cert_filename], check=True)
        elif sys.platform.startswith('linux'):
            cert_path = f"/usr/local/share/ca-certificates/{cert_filename}.crt"
            with open(cert_path, 'wb') as f:
                f.write(cert_pem)
            subprocess.run(['sudo', 'update-ca-certificates'], check=True)

    def untrust_certificate(cert_filename):
        if os.name == 'nt':
            subprocess.run(['certutil', '-delstore', 'Root', cert_filename], check=True)
        elif sys.platform == 'darwin':
            subprocess.run(['sudo', 'security', 'delete-certificate', '-c', cert_filename], check=True)
        elif sys.platform.startswith('linux'):
            cert_path = f"/usr/local/share/ca-certificates/{cert_filename}.crt"
            if os.path.exists(cert_path):
                os.remove(cert_path)
            subprocess.run(['sudo', 'update-ca-certificates', '--fresh'], check=True)

    key_pem, cert_pem = generate_self_signed_cert()

    cert_filename = "localhost"
    with tempfile.NamedTemporaryFile(delete=False, suffix=".crt") as cert_file, \
         tempfile.NamedTemporaryFile(delete=False, suffix=".key") as key_file:
        
        cert_file.write(cert_pem)
        key_file.write(key_pem)
        
        cert_filepath = cert_file.name
        key_filepath = key_file.name

    if register_cert:
        trust_certificate(cert_pem, cert_filepath)

    server_address = ('', port)
    httpd = server_class(server_address, handler_class)

    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(certfile=cert_filepath, keyfile=key_filepath)

    httpd.socket = context.wrap_socket(httpd.socket, server_side=True)

    hostname = socket.gethostname()
    ipv4_address = socket.gethostbyname(hostname)

    def cleanup_and_exit():
        if register_cert:
            untrust_certificate(cert_filename)
        
        httpd.server_close()
        sys.exit(0)

    def signal_handler(signal, frame):
        print('\nKeyboard interrupt received, exiting.')
        cleanup_and_exit()

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)
  
    print(f'Serving HTTPS on {ipv4_address} port {port} (https://{ipv4_address}:{port}/) ...')
    try:
        httpd.serve_forever()
    except Exception as e:
        print(f"Server error: {e}")
        cleanup_and_exit()

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Run an HTTP/HTTPS server with optional certificate registration.")
    parser.add_argument('--port', type=int, help="Port to run the server on (default: 443 for HTTPS, 80 for HTTP)")
    parser.add_argument('--mode', type=str, choices=['http', 'https'], default='http', help="Mode to run the server in: http or https (default: http)")
    parser.add_argument('--register', action='store_true', help="Register the certificate in the system's trust store (HTTPS mode only)")
    args = parser.parse_args()

    if args.mode == 'https':
        run_https(port=args.port or 443, register_cert=args.register)
    else:
        run_http(port=args.port or 80)
