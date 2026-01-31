#!/usr/bin/env python3
"""
echo_server.py - Simple WebSocket echo server for testing

This server echoes back any messages it receives, which is perfect
for testing the TallyWebSocket client implementation.

Requirements:
    pip install websockets

Usage:
    python3 echo_server.py [--port PORT] [--host HOST]

Default: ws://localhost:8080
"""

import asyncio
import argparse
import logging
from typing import Set
import signal
import sys

try:
    import websockets
    from websockets.server import WebSocketServerProtocol
except ImportError:
    print("Error: websockets library not found")
    print("Install with: pip install websockets")
    sys.exit(1)

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)
logger = logging.getLogger(__name__)

# Track connected clients
connected_clients: Set[WebSocketServerProtocol] = set()


async def echo_handler(websocket: WebSocketServerProtocol, path: str):
    """Handle WebSocket connections and echo messages back"""
    client_id = f"{websocket.remote_address[0]}:{websocket.remote_address[1]}"
    logger.info(f"Client connected: {client_id} (path: {path})")
    connected_clients.add(websocket)

    try:
        async for message in websocket:
            # Echo the message back
            if isinstance(message, bytes):
                logger.debug(f"Echoing binary message ({len(message)} bytes) to {client_id}")
                await websocket.send(message)
            else:
                logger.debug(f"Echoing text message ({len(message)} chars) to {client_id}: {message[:50]}")
                await websocket.send(message)

    except websockets.exceptions.ConnectionClosedOK:
        logger.info(f"Client disconnected normally: {client_id}")
    except websockets.exceptions.ConnectionClosedError as e:
        logger.warning(f"Client disconnected with error: {client_id} - {e}")
    except Exception as e:
        logger.error(f"Error handling client {client_id}: {e}")
    finally:
        connected_clients.discard(websocket)
        logger.info(f"Client removed: {client_id} (active connections: {len(connected_clients)})")


async def serve(host: str, port: int):
    """Start the WebSocket server"""
    logger.info(f"Starting WebSocket echo server on ws://{host}:{port}")
    logger.info("Press Ctrl+C to stop")

    async with websockets.serve(echo_handler, host, port):
        await asyncio.Future()  # Run forever


def signal_handler(sig, frame):
    """Handle Ctrl+C gracefully"""
    logger.info("\nShutting down server...")
    sys.exit(0)


def main():
    parser = argparse.ArgumentParser(
        description="WebSocket echo server for TallyWebSocket testing"
    )
    parser.add_argument(
        "--host",
        default="localhost",
        help="Host to bind to (default: localhost)"
    )
    parser.add_argument(
        "--port",
        type=int,
        default=8080,
        help="Port to listen on (default: 8080)"
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Enable verbose logging"
    )

    args = parser.parse_args()

    if args.verbose:
        logger.setLevel(logging.DEBUG)

    # Register signal handler for graceful shutdown
    signal.signal(signal.SIGINT, signal_handler)

    # Run the server
    try:
        asyncio.run(serve(args.host, args.port))
    except KeyboardInterrupt:
        logger.info("\nServer stopped by user")
    except Exception as e:
        logger.error(f"Server error: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()