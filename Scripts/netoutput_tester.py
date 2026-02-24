# Supermodel
# A Sega Model 3 Arcade Emulator.
# Copyright 2003-2026 The Supermodel Team
#
# This file is part of Supermodel.
#
# Supermodel is free software: you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free
# Software Foundation, either version 3 of the License, or (at your option)
# any later version.
#
# Supermodel is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License along
# with Supermodel.  If not, see <http://www.gnu.org/licenses/>.

###############################################################################
# Network output tester for Supermodel - receives messages via UDP and TCP.
#
# This script is designed to test the network output functionality of
# Supermodel by listening for messages sent by the emulator and processing
# them.
# It can be used to verify that Supermodel is sending the correct messages in
# response to various events (e.g., starting/stopping the emulator, pausing,
# etc.).
#
# Usage:
#  python netoutput_tester.py [--udp-port UDP_PORT] [--tcp-host TCP_HOST]
#                           [--tcp-port TCP_PORT] [--max-runtime MAX_RUNTIME]
#
#  --tcp-host TCP_HOST       TCP host to connect to (default: localhost)
#  --tcp-port TCP_PORT       TCP port to connect to (default: 8000)
#  --udp-port UDP_PORT       UDP port to listen on (default: 8001)
#  --max-runtime MAX_RUNTIME Maximum runtime in seconds (default: run forever)
#
# The script will print received messages to the console and can be stopped
# with Ctrl+C.
###############################################################################


import argparse
import logging
import signal
import socket
import sys
import threading
import time
from collections.abc import Callable
from types import FrameType

# Configure logging.
logging.basicConfig(level=logging.INFO, format="%(asctime)s - %(name)s - %(levelname)s - %(message)s")
LOGGER: logging.Logger = logging.getLogger(__name__)


class MessageProcessor:
    """Processes incoming messages from UDP and TCP connections.

    Handles both static keys (with dedicated handler functions) and dynamic keys
    (with a generic handler function).
    """

    def __init__(self) -> None:
        """Initialize the message processor with static key handlers."""
        self._static_handlers: dict[str, Callable[[str | None, str], None]] = {
            "mame_start": self._handle_mame_start,
            "pause": self._handle_pause,
            "mame_stop": self._handle_mame_stop,
            "tcp": self._handle_tcp,
        }

    def parse_message(self, message: str) -> tuple[str, str | None]:
        """Parse a message into key and optional value.

        Args:
            message (str): Raw message string to parse.

        Returns:
            tuple[str, str | None]: A tuple containing the key and optional value.
                Value is None if not present.
        """
        # Strip blanks from the line.
        message = message.strip()

        key: str
        value: str | None
        if "=" in message:
            # Split by '=' and strip blanks from both parts.
            parts: list[str] = message.split("=", 1)
            key = parts[0].strip()
            value = parts[1].strip() if len(parts) > 1 else None
        else:
            # No value, just a key.
            key = message
            value = None

        return key, value

    def process_message(self, message: str, source: str) -> None:
        """Process a received message by delegating to appropriate handler.

        Args:
            message (str): The message string to process.
            source (str): The source of the message (e.g., "UDP" or "TCP").
        """
        if not message:
            return

        key: str
        value: str | None
        key, value = self.parse_message(message)

        if not key:
            return

        # Check if it's a static key.
        if key in self._static_handlers:
            LOGGER.debug("Processing static key from %s: %s = %s", source, key, value)
            self._static_handlers[key](value, source)
        else:
            # Handle as dynamic key.
            LOGGER.debug("Processing dynamic key from %s: %s = %s", source, key, value)
            self._handle_dynamic_key(key, value, source)

    def _handle_mame_start(self, value: str | None, source: str) -> None:
        """Handle the mame_start command.

        Args:
            value (str | None): Optional value associated with the command.
            source (str): The source of the message (e.g., "UDP" or "TCP").
        """
        LOGGER.info("MAME START command received from %s with value: %s", source, value)

    def _handle_pause(self, value: str | None, source: str) -> None:
        """Handle the pause command.

        Args:
            value (str | None): Optional value associated with the command.
            source (str): The source of the message (e.g., "UDP" or "TCP").
        """
        LOGGER.info("PAUSE command received from %s with value: %s", source, value)

    def _handle_mame_stop(self, value: str | None, source: str) -> None:
        """Handle the mame_stop command.

        Args:
            value (str | None): Optional value associated with the command.
            source (str): The source of the message (e.g., "UDP" or "TCP").
        """
        LOGGER.info("MAME STOP command received from %s with value: %s", source, value)

    def _handle_tcp(self, value: str | None, source: str) -> None:
        """Handle the tcp command.

        Args:
            value (str | None): Optional value associated with the command.
            source (str): The source of the message (e.g., "UDP" or "TCP").
        """
        LOGGER.info("TCP command received from %s with value: %s", source, value)

    def _handle_dynamic_key(self, key: str, value: str | None, source: str) -> None:
        """Handle dynamic keys that are not in the static handler list.

        Args:
            key (str): The dynamic key name.
            value (str | None): Optional value associated with the key.
            source (str): The source of the message (e.g., "UDP" or "TCP").
        """
        LOGGER.info("Dynamic key '%s' received from %s with value: %s", key, source, value)


class UDPReceiver:
    """Receives and processes UDP messages on a configurable port."""

    def __init__(self, processor: MessageProcessor, port: int = 8001, buffer_size: int = 4096) -> None:
        """Initialize the UDP receiver.

        Args:
            processor (MessageProcessor): The message processor to handle received messages.
            port (int): The port to listen on (default: 8001).
            buffer_size (int): The buffer size for receiving data (default: 4096).
        """
        self._processor: MessageProcessor = processor
        self._port: int = port
        self._buffer_size: int = buffer_size
        self._running: bool = False
        self._thread: threading.Thread | None = None
        self._socket: socket.socket | None = None

    def start(self) -> None:
        """Start the UDP receiver in a separate thread."""
        if self._running:
            LOGGER.warning("UDP receiver is already running.")
            return

        self._running = True
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()
        LOGGER.info("UDP receiver started on port %d.", self._port)

    def stop(self) -> None:
        """Stop the UDP receiver."""
        if not self._running:
            return

        self._running = False
        if self._socket:
            self._socket.close()
        if self._thread:
            self._thread.join(timeout=5.0)
        LOGGER.info("UDP receiver stopped.")

    def _run(self) -> None:  # pylint: disable=too-many-branches  # noqa: PLR0912
        """Main loop for the UDP receiver thread."""
        try:
            # Create UDP socket.
            self._socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            self._socket.bind(("", self._port))
            LOGGER.info("UDP socket bound to port %d.", self._port)
            # Set a timeout to allow periodic checks for shutdown.
            self._socket.settimeout(0.1)

            # Buffer for incomplete messages.
            buffer: str = ""

            while self._running:
                try:
                    # Receive data.
                    data: bytes
                    addr: tuple[str, int]
                    data, addr = self._socket.recvfrom(self._buffer_size)
                    if not data:
                        continue

                    # Decode the data.
                    text: str = data.decode("utf-8", errors="ignore")
                    buffer += text

                    # Process complete lines.
                    while "\n" in buffer or "\r" in buffer:
                        # Split on both \r\n and \n.
                        line: str
                        if "\r\n" in buffer:
                            line, buffer = buffer.split("\r\n", 1)
                        elif "\n" in buffer:
                            line, buffer = buffer.split("\n", 1)
                        elif "\r" in buffer:
                            line, buffer = buffer.split("\r", 1)
                        else:
                            break

                        # Process the line.
                        if line:
                            LOGGER.debug("UDP received from %s: %s.", addr, line)
                            self._processor.process_message(line, source="UDP")

                except TimeoutError:
                    continue
                except OSError as e:
                    if self._running:
                        LOGGER.error("UDP socket error: %s", e)
                        break

        except Exception as e:  # pylint: disable=broad-except
            LOGGER.error("UDP receiver error: %s", e)
        finally:
            if self._socket:
                self._socket.close()


class TCPConnector:  # pylint: disable=too-many-instance-attributes
    """Connects to a TCP server and processes incoming messages."""

    def __init__(  # pylint: disable=too-many-positional-arguments, too-many-arguments
        self,
        processor: MessageProcessor,
        host: str = "localhost",
        port: int = 8000,
        retry_interval: float = 0.1,
        buffer_size: int = 4096,
    ) -> None:
        """Initialize the TCP connector.

        Args:
            processor (MessageProcessor): The message processor to handle received messages.
            host (str): The hostname to connect to (default: "localhost").
            port (int): The port to connect to (default: 8000).
            retry_interval (float): The interval in seconds between connection attempts
                (default: 0.1).
            buffer_size (int): The buffer size for receiving data (default: 4096).
        """
        self._processor: MessageProcessor = processor
        self._host: str = host
        self._port: int = port
        self._retry_interval: float = retry_interval
        self._buffer_size: int = buffer_size
        self._running: bool = False
        self._thread: threading.Thread | None = None
        self._socket: socket.socket | None = None

    def start(self) -> None:
        """Start the TCP connector in a separate thread."""
        if self._running:
            LOGGER.warning("TCP connector is already running.")
            return

        self._running = True
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()
        LOGGER.info("TCP connector started, will connect to %s:%d.", self._host, self._port)

    def stop(self) -> None:
        """Stop the TCP connector."""
        if not self._running:
            return

        self._running = False
        if self._socket:
            self._socket.close()
        if self._thread:
            self._thread.join(timeout=5.0)
        LOGGER.info("TCP connector stopped.")

    def _run(self) -> None:  # pylint: disable=too-many-branches  # noqa: PLR0912,PLR0915,C901
        """Main loop for the TCP connector thread."""
        LOGGER.info("Attempting to connect to %s:%d.", self._host, self._port)
        while self._running:  # pylint: disable=too-many-nested-blocks
            try:
                # Try to connect.
                self._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self._socket.settimeout(5.0)
                self._socket.connect((self._host, self._port))
                LOGGER.info("Connected to %s:%d.", self._host, self._port)

                # Buffer for incomplete messages.
                buffer: str = ""

                # Receive loop.
                while self._running:
                    try:
                        data: bytes = self._socket.recv(self._buffer_size)
                        if not data:
                            LOGGER.info("TCP connection closed by server.")
                            break

                        # Decode the data.
                        text: str = data.decode("utf-8", errors="ignore")
                        buffer += text

                        # Process complete lines.
                        while "\n" in buffer or "\r" in buffer:
                            # Split on both \r\n and \n.
                            line: str
                            for delimiter in ("\r\n", "\n", "\r"):
                                if delimiter in buffer:
                                    line, buffer = buffer.split(delimiter, 1)
                                    break
                            else:
                                break

                            # Process the line.
                            if line:
                                LOGGER.debug("TCP received: %s.", line)
                                self._processor.process_message(line, source="TCP")

                    except TimeoutError:
                        continue
                    except OSError as e:
                        if self._running:
                            LOGGER.error("TCP socket error: %s", e)
                            break

            except (TimeoutError, ConnectionRefusedError):
                if self._running:
                    time.sleep(self._retry_interval)
            except Exception as e:  # pylint: disable=broad-except
                if self._running:
                    LOGGER.error("TCP connection error: %s", e)
                    LOGGER.info("Retrying in %s seconds...", self._retry_interval)
                    time.sleep(self._retry_interval)
            finally:
                if self._socket:
                    self._socket.close()
                    self._socket = None


def parse_arguments() -> argparse.Namespace:
    """Parse command-line arguments.

    Returns:
        argparse.Namespace: Parsed command-line arguments.
    """
    parser: argparse.ArgumentParser = argparse.ArgumentParser(
        description="Network output tester for Supermodel - receives messages via UDP and TCP."
    )

    parser.add_argument("--udp-port", type=int, default=8001, help="UDP port to listen on (default: 8001)")

    parser.add_argument("--tcp-host", type=str, default="localhost", help="TCP host to connect to (default: localhost)")

    parser.add_argument("--tcp-port", type=int, default=8000, help="TCP port to connect to (default: 8000)")

    parser.add_argument(
        "--max-runtime", type=int, default=None, help="Maximum runtime in seconds (default: run forever)"
    )

    return parser.parse_args()


def main() -> None:
    """Main entry point for the network output tester."""
    print("  ####                                                      ###           ###")
    print(" ##  ##                                                      ##            ##")
    print(" ###     ##  ##  ## ###   ####   ## ###  ##  ##   ####       ##   ####     ##")
    print("  ###    ##  ##   ##  ## ##  ##   ### ## ####### ##  ##   #####  ##  ##    ##")
    print("    ###  ##  ##   ##  ## ######   ##  ## ####### ##  ##  ##  ##  ######    ##")
    print(" ##  ##  ##  ##   #####  ##       ##     ## # ## ##  ##  ##  ##  ##        ##")
    print("  ####    ### ##  ##      ####   ####    ##   ##  ####    ### ##  ####    ####")
    print("                 ####")
    print("")
    print("Network Output Tester v1.0.0")
    print()

    # Parse command-line arguments.
    args: argparse.Namespace = parse_arguments()

    # Create message processor.
    processor: MessageProcessor = MessageProcessor()

    # Create and start UDP receiver.
    udp_receiver: UDPReceiver = UDPReceiver(processor, port=args.udp_port)
    udp_receiver.start()

    # Create and start TCP connector.
    tcp_connector: TCPConnector = TCPConnector(processor, host=args.tcp_host, port=args.tcp_port)
    tcp_connector.start()

    # Setup signal handler for immediate exit on Ctrl-C.
    def signal_handler(sig: int, frame: FrameType | None) -> None:
        """Handle SIGINT for immediate shutdown.

        Args:
            sig (int): The signal number.
            frame (FrameType | None): The current stack frame.
        """
        _ = sig, frame  # Unused parameters.
        LOGGER.info("\nShutting down...")
        udp_receiver.stop()
        tcp_connector.stop()
        LOGGER.info("Shutdown complete.")
        sys.exit(0)

    signal.signal(signal.SIGINT, signal_handler)

    if args.max_runtime:
        LOGGER.info("Network output tester is running for %d seconds. Press Ctrl+C to stop.", args.max_runtime)
    else:
        LOGGER.info("Network output tester is running. Press Ctrl+C to stop.")

    # Keep the main thread alive.
    start_time: float = time.time()
    while True:
        # Check if maximum runtime has been exceeded.
        if args.max_runtime:
            elapsed: float = time.time() - start_time
            if elapsed >= args.max_runtime:
                LOGGER.info("Maximum runtime of %d seconds reached. Shutting down...", args.max_runtime)
                udp_receiver.stop()
                tcp_connector.stop()
                break

        time.sleep(0.1)


if __name__ == "__main__":
    main()
