#pragma once

#include "cursor.h"
#include "str.h"

typedef enum {
  HTTP_METHOD_GET,
  HTTP_METHOD_POST,
  HTTP_METHOD_PUT,
  HTTP_METHOD_PATCH,
  HTTP_METHOD_HEAD,
  HTTP_METHOD_DELETE,
} Method;

typedef struct {
  Str key, value;
} Header;

// TODO: headers.
typedef struct {
  Method method;
  int error;
  Str hostname;
  Str path;
  Str params;
  Str body;
} Request;

// TODO: headers.
typedef struct {
  u16 status;
  pg_pad(6);
  Str body;
} Response;

static Request parse_request(Read_result read_res) {
  if (read_res.error) {
    return (Request){.error = read_res.error};
  }

  Read_cursor cursor = {.s = read_res.content};
  Request req = {0};

  {
    if (read_cursor_match(&cursor, str_from_c("GET"))) {
      req.method = HTTP_METHOD_GET;
    } else if (read_cursor_match(&cursor, str_from_c("POST"))) {
      req.method = HTTP_METHOD_POST;
    } else if (read_cursor_match(&cursor, str_from_c("PATCH"))) {
      req.method = HTTP_METHOD_PATCH;
    } else if (read_cursor_match(&cursor, str_from_c("PUT"))) {
      req.method = HTTP_METHOD_PUT;
    } else if (read_cursor_match(&cursor, str_from_c("DELETE"))) {
      req.method = HTTP_METHOD_DELETE;
    } else if (read_cursor_match(&cursor, str_from_c("HEAD"))) {
      req.method = HTTP_METHOD_HEAD;
    } else {
      return (Request){.error = true};
    }
  }

  if (!read_cursor_match(&cursor, str_from_c(" "))) {
    return (Request){.error = true};
  }

  const Str url = read_cursor_match_until_excl(&cursor, ' ');
  if (str_is_empty(url)) {
    return (Request){.error = true};
  }
  // TODO: Parse url.

  if (!read_cursor_match(&cursor, str_from_c("HTTP/1.1\r\n"))) {
    return (Request){.error = true};
  }

  if (!read_cursor_match(&cursor, str_from_c("\r\n"))) {
    return (Request){.error = true};
  }

  return req;
}
