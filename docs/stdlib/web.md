# web (HTTP server)

```
use web
```

## Functions

| Function | Description |
|----------|-------------|
| `web.serve(port, handler)` | Start HTTP server |
| `web.json(value)` | Convert value to JSON string |
| `web.parse_json(string)` | Parse JSON string to value |

## Starting a Server

```
use web

to handle(request):
  return {status = 200, body = "Hello!"}

web.serve(8080, handle)
```

The handler receives a request dict and must return a response.

## Request Object

| Field | Type | Description |
|-------|------|-------------|
| `request.method` | string | `"GET"`, `"POST"`, etc. |
| `request.path` | string | `"/api/users"` |
| `request.headers` | dict | HTTP headers (lowercase keys) |
| `request.params` | dict | Query parameters |
| `request.body` | string | Request body |

## Response Format

Return a string for plain text, or a dict for full control:

```
~ Plain text
return "Hello!"

~ Full response
return {
  status = 200,
  body = "<h1>Hello</h1>",
  type = "text/html"
}

~ JSON (auto-serialized from dict/list)
return {
  status = 200,
  body = {name = "Theo", age = 18}
}

~ Custom headers
return {
  status = 200,
  body = "data",
  headers = {x_custom = "value"}
}
```

## Example: REST API

```
use web

users = [
  {name = "Alice", age = 30},
  {name = "Bob", age = 25}
]

to handle(req):
  if req.path == "/api/users":
    return {status = 200, body = users}

  if req.path == "/":
    return {status = 200, body = "<h1>API</h1>", type = "text/html"}

  return {status = 404, body = "Not found"}

web.serve(3000, handle)
```

The server is multi-threaded — each request runs in its own thread.
