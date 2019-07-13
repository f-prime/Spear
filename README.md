# Fist - (F)ull-(t)ext (i)ndex (s)erver 

[![Slack](https://img.shields.io/badge/Slack-Join%20the%20slack%20channel-BLUE.svg)](https://join.slack.com/t/fist-global/shared_invite/enQtNjcyNzY4MTUwMDg0LTRiYzM5ZWNkOTMwODYzODRjNDQzNThiYjdhNjgzZDUxZGYxODRjOTI4NTcwYmYzYmI5MTViYjFiNGFlNWEwYjY)
[![Patreon](https://img.shields.io/badge/Patreon-Help%20fund%20this%20project-RED.svg)](https://www.patreon.com/fistproject)
[![CircleCI](https://circleci.com/gh/f-prime/fist.svg?style=svg)](https://circleci.com/gh/f-prime/fist)

Fist is a fast, lightweight, full-text search and index server. Fist stores all information in memory making lookups very fast while also persisting the index to disk. The index can be accessed over a TCP connection and all data returned is valid JSON.

**Fist is still heavily under development. Not all features are implemented or stable yet.**

# Motivation

Most software that requires full-text search is not really that complicated and does not need an overly complex solution. Using a complex solution often times leads to headaches. 
Setting up Elasticsearch when Elasticsearch really isn't needed for the particular application costs more time and money to maintain. 

This is where Fist comes in. Fist is intended to be extremely easy to deploy and integrate into your application. Just start the Fist server and start sending commands.

# Build and start Fist server

```
make
./bin/fist
Fist started at localhost:5575
```

# Run Tests

```
make test
```

# Example Usage

Commands can be sent over a TELNET connection

Commands: `INDEX`, `SEARCH`, `EXIT`, `VERSION`, `DELETE`

```
telnet localhost 5575
Trying ::1...
telnet: connect to address ::1: Connection refused
Trying 127.0.0.1...
Connected to localhost.
Escape character is '^]'.
INDEX document_1 Some text that I want to index
Text has been indexed
INDEX document_2 Some other text that I want to index
Text has been indexed
SEARCH I want to index
["document_1","document_2"]
DELETE I want to index
Key Deleted
SEARCH I want to index
[]
EXIT
Bye
```

# Docker Usage

```
# Build image
docker build . -t fist:latest

# Run tests
docker run --rm -it fist test

# Run server and make volume for database
docker run -d --init --rm -p 5575:5575 -v /var/local/lib/fist fist
```

# Key Features

- Full text indexing and searching
- Persisting data to disk
- Compression of index file
- Accessible over TCP connection

# Client Libraries

### NodeJS

- [node-fist](https://github.com/00-matt/node-fist)

### Python

- [fistpy](https://github.com/puria/fistpy)

### Go

- [go-fist](https://github.com/sonirico/go-fist)

# Contributors

- [@AndreRenaud](https://github.com/AndreRenaud)
- [@00-matt](https://github.com/00-matt)
- [@io-ma](https://github.com/io-ma)
