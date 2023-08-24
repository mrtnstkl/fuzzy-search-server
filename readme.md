# Fuzzy Search Server

## Overview

A simple HTTP server that performs fuzzy searches on a dataset loaded from a text file.

Uses [cpp-httplib](https://github.com/yhirose/cpp-httplib) and [nlohmann/json](https://github.com/nlohmann/json).

## Usage

```bash
./fuzzy-search-server DATASET... [-p PORT] [-bi | -tri | -tetra]
```

- `DATASET`: The paths to the text files containing the data entries. Each line should be a separate JSON object with at least a "name" field.
- `PORT` (optional): The port number on which the server should listen. Defaults to `8080`.
- `-bi | -tri | -tetra` (optional): The n-gram-size used by the fuzzy search. Defaults to `-bi`. Higher sizes can drastically improve speed, but might miss out on some more distant matches.

## API

See [api.md](api.md)

## Example

For a JSON file `parks.json` containing:
```
{"name": "Hyde Park", "city": "London", "lat": 51.507327, "lon": -0.169644}
{"name": "Central Park", "city": "New York", "lat": 40.7825, "lon": -73.966111}
...
```

Run:
```bash
./fuzzy-search-server parks.json -p 1234
```

Query:
```
http://localhost:1234/fuzzy?q=centrl%20bark
```

Response:
```json
{"name": "Central Park", "city": "New York", "lat": 40.7825, "lon": -73.966111}
```
