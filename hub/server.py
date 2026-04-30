#!/usr/bin/env python3
"""
OTA Hub Server
Runs on Raspberry Pi to manage ESP32 OTA updates
"""

from flask import Flask, request, jsonify, render_template, send_from_directory
import os
import json
import uuid
from datetime import datetime

app = Flask(__name__)

# Configuration
FIRMWARE_DIR = 'firmware'
NODE_DB_FILE = 'nodes.json'

# Ensure directories exist
os.makedirs(FIRMWARE_DIR, exist_ok=True)

# In-memory node database
nodes = {}

def load_nodes():
    """Load node database from file"""
    global nodes
    if os.path.exists(NODE_DB_FILE):
        with open(NODE_DB_FILE, 'r') as f:
            nodes = json.load(f)

def save_nodes():
    """Save node database to file"""
    with open(NODE_DB_FILE, 'w') as f:
        json.dump(nodes, f)

def register_node(node_id, version):
    """Register or update a node"""
    if node_id not in nodes:
        nodes[node_id] = {
            'id': node_id,
            'version': version,
            'registered': datetime.now().isoformat(),
            'last_seen': datetime.now().isoformat(),
            'status': 'online',
            'pending_update': False,
            'firmware_url': None
        }
    else:
        nodes[node_id]['last_seen'] = datetime.now().isoformat()
        nodes[node_id]['status'] = 'online'
    save_nodes()

@app.route('/')
def index():
    """Web dashboard"""
    return render_template('index.html', nodes=nodes)

@app.route('/api/nodes', methods=['GET'])
def api_nodes_check():
    """
    Node checks for updates
    GET /api/nodes?id=node-001&version=1.0.0
    """
    node_id = request.args.get('id', 'unknown')
    version = request.args.get('version', 'unknown')
    
    # Register node
    register_node(node_id, version)
    
    # Check if update pending
    node = nodes.get(node_id, {})
    pending = node.get('pending_update', False)
    
    response = {
        'id': node_id,
        'update_available': pending,
        'current_version': version
    }
    
    if pending:
        response['firmware_url'] = f"/firmware/{node_id}.bin"
        response['firmware_version'] = node.get('pending_version', 'unknown')
        response['firmware_size'] = node.get('firmware_size', 0)
    
    return jsonify(response)

@app.route('/api/confirm', methods=['GET'])
def api_confirm():
    """Confirm update was successful"""
    node_id = request.args.get('id')
    version = request.args.get('version')
    
    if node_id in nodes:
        nodes[node_id]['version'] = version
        nodes[node_id]['pending_update'] = False
        nodes[node_id]['confirmed'] = datetime.now().isoformat()
        save_nodes()
    
    return jsonify({'ok': True})

@app.route('/api/status', methods=['POST'])
def api_status():
    """Node reports status"""
    data = request.json
    node_id = data.get('id')
    
    if node_id in nodes:
        nodes[node_id]['status'] = data.get('status', 'online')
        nodes[node_id]['last_seen'] = datetime.now().isoformat()
        save_nodes()
    
    return jsonify({'ok': True})

@app.route('/upload', methods=['GET', 'POST'])
def upload():
    """Upload firmware page and handler"""
    if request.method == 'POST':
        # Get target node
        node_id = request.form.get('node_id')
        
        # Get uploaded file
        file = request.files.get('firmware')
        if not file:
            return "No file uploaded", 400
        
        # Save firmware
        filename = f"{node_id}.bin"
        filepath = os.path.join(FIRMWARE_DIR, filename)
        file.save(filepath)
        
        file_size = os.path.getsize(filepath)
        
        # Get version
        version = request.form.get('version', '1.0.0')
        
        # Mark update pending
        if node_id in nodes:
            nodes[node_id]['pending_update'] = True
            nodes[node_id]['pending_version'] = version
            nodes[node_id]['firmware_size'] = file_size
        else:
            nodes[node_id] = {
                'id': node_id,
                'pending_update': True,
                'pending_version': version,
                'firmware_size': file_size
            }
        
        save_nodes()
        
        return f"Firmware uploaded for {node_id}.bin (Size: {file_size} bytes)"
    
    return render_template('upload.html', nodes=nodes)

@app.route('/firmware/')
def download_firmware(filename):
    """Download firmware file"""
    return send_from_directory(FIRMWARE_DIR, filename)

@app.route('/api/nodes/<node_id>', methods=['DELETE'])
def delete_node(node_id):
    """Remove a node"""
    if node_id in nodes:
        del nodes[node_id]
        save_nodes()
    return jsonify({'ok': True})

@app.route('/api/firmware/<node_id>', methods=['DELETE'])
def delete_firmware(node_id):
    """Delete pending firmware for node"""
    filename = f"{node_id}.bin"
    filepath = os.path.join(FIRMWARE_DIR, filename)
    
    if os.path.exists(filepath):
        os.remove(filepath)
    
    if node_id in nodes:
        nodes[node_id]['pending_update'] = False
        nodes[node_id]['pending_version'] = None
        save_nodes()
    
    return jsonify({'ok': True})

if __name__ == '__main__':
    load_nodes()
    print("="*50)
    print("  OTA Hub Server Starting...")
    print("  Connect to: http://raspberrypi.local:5000")
    print("="*50)
    app.run(host='0.0.0.0', port=5000, debug=True)