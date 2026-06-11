# Fuzzy Search Server

## Overview

A simple HTTP server that performs fuzzy searches on a dataset loaded from a text file.

Uses [cpp-httplib](https://github.com/yhirose/cpp-httplib) and [nlohmann/json](https://github.com/nlohmann/json).

### Demo

I host an **[interactive demo](https://gnorts.xyz/osmpoidemo/)** on my website. The fuzzy-search-server acts as the backend.

## Usage

```
./fuzzy-search-server DATASET... [-p PORT] [-c CONFIG_FILE] [-nf NAME_FIELD] [-l RESULT_LIMIT]
            [-bc BUCKET_CAPACITY] [-bi | -tri | -tetra] [-fl] [-disk] [-dc]
```

- `DATASET`: The paths to the text files containing the data entries. Each line should be a separate JSON object with at least a name field.
- `PORT` (optional): The port number on which the server should listen. Defaults to `8080`.
- `CONFIG_FILE` (optional): The path to a JSON configuration file. If provided, the server will be configured according to the settings in this file instead of (or in addition to) using command-line options.
- `NAME_FIELD` (optional): The json field that contains the search key. Default is "name". Each dataset entry should have this field. This is a regex, so you may specify something like `"name(:.+)?"` to match "name", "name:en", "name:de", etc. If multiple fields match, all of them will be used for searching.
- `RESULT_LIMIT` (optional): Allows you to enforce a maximum page size for result lists. Default is `100`. Negative values or zero will remove the limit.
- `BUCKET_CAPACITY` (optional): The maximum number of elements that can be associated with a specific n-gram. If an n-gram exceeds this limit, it will no longer be used for matching. This greatly improves performance for datasets with many identical substrings. Default is `10000`. Negative values or zero will remove the limit.
- `-bi | -tri | -tetra` (optional): The n-gram-size used by the fuzzy search. Defaults to `-bi`. Higher sizes can drastically improve speed, but might miss out on some more distant matches.
- `-fl` (optional): If set, fuzzy search will only consider elements that start with the same letter. This improves performance.
- `-disk` (optional): If set, only element names will be kept in memory. So when elements are requested, they will be read from disk. Reduces memory use (especially for datasets with large JSON objects) at the cost of performance.
- `-dc` (optional): If set, lines with identical string hashes will only be included once.

### Configuration file

Configuration files allow you to set up multiple search modules with different settings and datasets. They also allow you to specify settings on a per-dataset basis, which can be useful if you have datasets with different characteristics. The configuration file should be a JSON array of objects, each representing a search module. Each object can have the following properties:

- `baseUrl` (string, optional): The base URL for the module. For example, if `baseUrl` is set to "pois", the module will be accessible at `/pois`. If not provided, the module will be accessible at the root URL (`/`).
- `nGram` (string, optional): The n-gram size to use for the module. Can be "bi", "tri", or "tetra".
- `bucketCap` (integer, optional): See `BUCKET_CAPACITY` above.
- `maxResults` (integer, optional): See `RESULT_LIMIT` above.
- `allowDuplicates` (boolean, optional): Whether the module should allow duplicate entries. Analogous to `-dc` above.
- `disk` (boolean, optional): See `-disk` above. This sets the default for all datasets in the module, but can be overridden on a per-dataset basis.
- `key` (string, optional): See `NAME_FIELD` above. This sets the default for all datasets in the module, but can be overridden on a per-dataset basis.
- `datasets` (array of strings or objects, required): The datasets to load for the module. Each dataset can be specified as a string (the path to the dataset file) or as an object with the following properties:
  - `path` (string, required): The path to the dataset file.
  - `disk` (boolean, optional): Override the module's default for this dataset.
  - `key` (string, optional): The name field to use for the dataset.

Unspecified settings will fall back to the command-line options or their defaults.

## API

See [api.md](api.md)

## Example

For a file `parks.txt` containing:
```
{"name": "Hyde Park", "city": "London", "lat": 51.507327, "lon": -0.169644}
{"name": "Central Park", "city": "New York", "lat": 40.7825, "lon": -73.966111}
...
```

Run:
```bash
./fuzzy-search-server parks.txt -p 1234
```

Query:
```
http://localhost:1234/fuzzy?q=centrl%20bark
```

Response:
```json
{"name": "Central Park", "city": "New York", "lat": 40.7825, "lon": -73.966111}
```
