# Fuzzy Search Server

## Overview

A simple HTTP server that performs fuzzy searches on a dataset loaded from a text file.

Uses [cpp-httplib](https://github.com/yhirose/cpp-httplib) and [nlohmann/json](https://github.com/nlohmann/json).

## Usage

```bash
./fuzzy-search-server INFILE [PORT]
```

- `INFILE`: The path to the text file containing the database entries. Each line should be a separate JSON object with at least a "name" field.
- `PORT` (Optional): The port number on which the server should listen. Defaults to `8080` if not provided.

## API Endpoints

### 1. Fuzzy Search

Searches for entries with the most similar names. Similarity is determined by calculating the optimal string alignment distance.

`GET /fuzzy`

**Parameters:**
- `q`: The search term
- `list` (optional): `yes` to get a list of the best matches. `no` to get one of the best matches. Default is `no`.


### 2. Exact Search

Only searches for exact matches.

`GET /exact`

**Parameters:**
- `q`: The search term
- `list` (optional): `yes` to get a list of all exact matches. `no` to get one of the exact matches. Default is `no`.

### 3. Completion Search

Searches for entries that start with a given string.

`GET /complete`

**Parameters:**
- `q`: The search term
- `list` (optional): `yes` to get a sorted list of entries that start with the search term. `no` to get the lexicographically smallest entry that starts with the search term. Default is `yes`.

## Example

For a JSON file `parks.json` containing:
```
{"name": "Hyde Park", "city": "London", "lat": 51.507327, "lon": -0.169644}
{"name": "Central Park", "city": "New York", "lat": 40.7825, "lon": -73.966111}
...
```

Run:
```bash
./fuzzy-search-server parks.json 1234
```

Query:
```
http://localhost:1234/fuzzy?q=centrl%20bark
```

Response:
```json
{"name": "Central Park", "city": "New York", "lat": 40.7825, "lon": -73.966111}
```
