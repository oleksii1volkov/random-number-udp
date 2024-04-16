# Random Number UDP Server-Client

This project includes a UDP server that generates random numbers and sends them to a client. The client receives these numbers, sorts them in descending order, and stores them in a file.

## Project Structure

- `src/server/main.cpp`: UDP server that generates and sends random numbers.
- `src/client.main.cpp`: UDP client that receives numbers, sorts them, and writes to a file.

## Setup and Running

### Prerequisites

- Python 3.x
- CMake 3.21
- Conan 2.2.2
- MSVC 193, Clang 18

### Installation

1. Clone the repository to your local machine:
   ```bash
   git clone https://github.com/your-repository/random-number-udp.git
   cd random-number-udp

2. Run `.\install_prerequisites.sh` to install dependencies.

3. Run `.\build.sh Release` to build the project.

### Execution

1. Run `.\client.sh Release` to run the client.

2. Run `.\server.sh Release` to run the server.

Tested on Windwos with MSVC 193 and on Linux with Clang 18.
