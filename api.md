# Fuzzy Search Server API

## Fuzzy Search

Searches for entries with the most similar names. Similarity is determined by calculating the optimal string alignment distance.

### `GET /fuzzy`

Gets one of the best matches.

**Parameters:**
- `q`: The search term.

### `GET /fuzzy/list`

Gets a list of best matches.

**Parameters:**
- `q`: The search term.
- `[count]`: The max amount of returned elements. Default is `10`. If negative or zero, count will be set to infinity.

---

## Fuzzycomplete Search

Searches for entries that have a similar beginning. Similarity is determined by calculating the optimal string alignment distance. Well suited for search fields that yield results as you type.

### `GET /fuzzycomplete`

Out of the entries with the most similar beginning, it gets one that is closest in length to the search term.

**Parameters:**
- `q`: The search term.

### `GET /fuzzycomplete/list`

Gets a sorted list of entries with a similar beginning. The shortest, most similar entry comes first.

**Parameters:**
- `q`: The search term.
- `[tol]`: The similarity tolerance, which limits the amount of results by only allowing ones that are at most `tol` away from the best match. Default is `2`.

---

## Exact Search

Only searches for exact matches.

### `GET /exact`

Gets one of the exact matches.

**Parameters:**
- `q`: The search term.

### `GET /exact/list`

Gets a paged list of exact matches.

**Parameters:**
- `q`: The search term.
- `[page]`: The page number. Default is `0`.
- `[count]`: The page size. The returned list will contain at most this many elements. Default is `10`. If negative or zero, page size will be set to infinity.


---

## Completion Search

Searches for entries that start with a given string.

### `GET /complete`

Gets the lexicographically smallest entry that starts with the search term.

**Parameters:**
- `q`: The search term.

### `GET /complete/list`

Gets a paged, lexicographically sorted list of entries that start with the search term.

**Parameters:**
- `q`: The search term.
- `[page]`: The page number. Default is `0`.
- `[count]`: The page size. The returned list will contain at most this many elements. Default is `10`. If negative or zero, page size will be set to infinity.

