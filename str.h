#pragma once

#include "arena.h"
#include "array.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

// String builder, like a dynamic array.
typedef struct {
  u8 *data;
  usize len;
  usize cap;
} Str_builder;

// String view, immutable.
typedef struct {
  u8 *data;
  usize len;
} Str;
Array_struct(Str);

__attribute__((warn_unused_result)) static u32 str_count(Str s, u8 c) {
  u32 res = 0;

  for (usize i = 0; i < s.len; i++) {
    if (s.data[i] == c) {
      res += 1;
    }
  }
  return res;
}

__attribute__((warn_unused_result)) static Str str_from_c(char *s) {
  return (Str){.data = (u8 *)s, .len = s == NULL ? 0 : strlen(s)};
}

#define str_from_c_literal(s) ((Str){.data = (u8 *)s, .len = sizeof(s) - 1})

__attribute__((warn_unused_result)) static u8 *ut_memrchr(u8 *s, u8 c,
                                                          usize n) {
  pg_assert(s != NULL);
  pg_assert(n > 0);

  u8 *res = s + n - 1;
  while (res-- != s) {
    if (*res == c)
      return res;
  }
  return NULL;
}

// https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
__attribute__((warn_unused_result)) static usize ut_next_power_of_two(usize n) {
  n += !n; // Ensure n>0

  n--;
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
  n |= n >> 32;
  n++;

  return n;
}

__attribute__((warn_unused_result)) static Str str_new(u8 *s, usize n) {
  return (Str){.data = s, .len = n};
}

__attribute__((warn_unused_result)) static Str str_clone(Str s, Arena *arena) {
  // Optimization: no alloc.
  if (s.len == 0)
    return s;

  u8 *data = arena_alloc(arena, sizeof(u8), _Alignof(u8), s.len);
  pg_assert(data);
  pg_assert(s.data);

  pg_assert(s.data);
  memmove(data, s.data, s.len);

  return (Str){.data = data, .len = s.len};
}

__attribute__((warn_unused_result)) static Str str_advance(Str s, usize n) {
  if (n > s.len)
    return (Str){0};
  return (Str){.data = s.data + n, .len = s.len - n};
}

__attribute__((warn_unused_result)) static u8 str_first(Str s) {
  return s.len > 0 ? s.data[0] : 0;
}

__attribute__((warn_unused_result)) static bool str_starts_with(Str haystack,
                                                                Str needle) {
  if (needle.len > haystack.len)
    return false;

  return memcmp(haystack.data, needle.data, needle.len) == 0;
}

__attribute__((warn_unused_result)) static bool str_ends_with(Str haystack,
                                                              Str needle) {
  if (needle.len > haystack.len)
    return false;

  usize start = haystack.len - needle.len;
  return memcmp(&haystack.data[start], needle.data, needle.len) == 0;
}

__attribute__((warn_unused_result)) static bool str_ends_with_c(Str haystack,
                                                                char *needle) {
  return str_ends_with(haystack, str_from_c(needle));
}

__attribute__((warn_unused_result)) static bool str_is_empty(Str s) {
  return s.len == 0;
}

__attribute__((warn_unused_result)) static bool str_eq(Str a, Str b) {
  if (a.len == 0 && b.len != 0)
    return false;
  if (b.len == 0 && a.len != 0)
    return false;

  if (a.len == 0 && b.len == 0)
    return true;

  pg_assert(a.data != NULL);
  pg_assert(b.data != NULL);

  return a.len == b.len && memcmp(a.data, b.data, a.len) == 0;
}

__attribute__((warn_unused_result)) static bool str_eq_c(Str a, char *b) {
  return str_eq(a, str_from_c(b));
}

__attribute__((warn_unused_result)) static bool
str_contains_element(Str haystack, u8 needle) {
  for (i64 i = 0; i < (i64)haystack.len - 1; i++) {
    if (haystack.data[i] == needle)
      return true;
  }
  return false;
}

__attribute__((warn_unused_result)) static isize str_find(Str haystack,
                                                          Str needle) {
  if (needle.len > haystack.len)
    return -1;

  for (usize i = 0; i < haystack.len - needle.len; i++) {
    Str remaining = str_advance(haystack, i);
    if (str_starts_with(remaining, needle)) {
      return (isize)i;
    }
  }
  return -1;
}

__attribute__((warn_unused_result)) static Str str_trim_left(Str s, u8 c) {
  Str remaining = s;
  while (str_first(remaining) == c) {
    remaining = str_advance(remaining, 1);
  }

  return remaining;
}

typedef struct {
  Str left, right;
  usize found_pos;
  bool found;
  pg_pad(7);
} Str_split_result;

__attribute__((warn_unused_result)) static Str_split_result
str_split(Str haystack, u8 needle) {
  u8 *at = memchr(haystack.data, needle, haystack.len);
  if (at == NULL)
    return (Str_split_result){.left = haystack, .right = haystack};

  usize found_pos = (usize)at - (usize)haystack.data;

  return (Str_split_result){
      .left = (Str){.data = haystack.data, .len = found_pos},
      .right = (Str){.data = at + 1, .len = haystack.len - found_pos - 1},
      .found_pos = found_pos,
      .found = true,
  };
}

__attribute__((warn_unused_result)) static Str_split_result
str_rsplit(Str haystack, u8 needle) {
  u8 *at = ut_memrchr(haystack.data, needle, haystack.len);
  if (at == NULL)
    return (Str_split_result){.left = haystack, .right = haystack};

  usize found_pos = (usize)at - (usize)haystack.data;

  return (Str_split_result){
      .left = (Str){.data = haystack.data, .len = found_pos},
      .right = (Str){.data = at + 1, .len = haystack.len - found_pos - 1},
      .found_pos = found_pos,
      .found = true,
  };
}

__attribute__((warn_unused_result)) static usize sb_space(Str_builder sb) {
  pg_assert(sb.len < sb.cap);

  return sb.cap - sb.len - 1;
}

__attribute__((warn_unused_result)) static Str_builder
sb_reserve_at_least(Str_builder sb, usize more, Arena *arena) {
  pg_assert(sb.len <= sb.cap);

  if (more <= sb_space(sb))
    return sb;

  usize new_cap = ut_next_power_of_two(sb.cap + more + 1 /* NUL terminator */);
  pg_assert(new_cap > sb.len);

  u8 *new_data = arena_alloc(arena, sizeof(u8), _Alignof(u8), new_cap);
  pg_assert(new_data);
  pg_assert(sb.data);

  if (sb.data)
    memmove(new_data, sb.data, sb.len);

  pg_assert(sb.data[sb.len] == 0);
  pg_assert(new_data[sb.len] == 0);

  return (Str_builder){.len = sb.len, .cap = new_cap, .data = new_data};
}

__attribute__((warn_unused_result)) static u8 *sb_end_c(Str_builder sb) {
  return sb.data + sb.len;
}

__attribute__((warn_unused_result)) static Str_builder
sb_append(Str_builder sb, Str more, Arena *arena) {
  sb = sb_reserve_at_least(sb, more.len, arena);

  if (more.data)
    memmove(sb_end_c(sb), more.data, more.len);

  sb.data[sb.len + more.len] = 0;
  return (Str_builder){
      .len = sb.len + more.len, .data = sb.data, .cap = sb.cap};
}

__attribute__((warn_unused_result)) static Str_builder
sb_append_c(Str_builder sb, char *more, Arena *arena) {
  return sb_append(sb, str_from_c(more), arena);
}

__attribute__((warn_unused_result)) static Str_builder
sb_append_char(Str_builder sb, u8 c, Arena *arena) {
  sb = sb_reserve_at_least(sb, 1, arena);
  sb.data[sb.len] = c;
  sb.data[sb.len + 1] = 0;
  return (Str_builder){.len = sb.len + 1, .data = sb.data, .cap = sb.cap};
}

__attribute__((warn_unused_result)) static Str_builder sb_new(usize initial_cap,
                                                              Arena *arena) {
  return (Str_builder){
      .data = arena_alloc(arena, sizeof(u8), _Alignof(u8), initial_cap + 1),
      .cap = initial_cap + 1,
  };
}

__attribute__((warn_unused_result)) static Str_builder
sb_assume_appended_n(Str_builder sb, usize more) {
  return (Str_builder){.len = sb.len + more, .data = sb.data, .cap = sb.cap};
}

__attribute__((warn_unused_result)) static Str sb_build(Str_builder sb) {
  pg_assert(sb.data);
  pg_assert(sb.cap > 0);
  pg_assert(sb.data[sb.len] == 0);

  return (Str){.data = sb.data, .len = sb.len};
}

__attribute__((warn_unused_result)) static Str_builder
sb_append_u64(Str_builder sb, usize n, Arena *arena) {
  char tmp[25] = "";
  snprintf(tmp, sizeof(tmp) - 1, "%lu", n);
  return sb_append_c(sb, tmp, arena);
}

__attribute__((warn_unused_result)) static Str_builder
sb_capitalize_at(Str_builder sb, usize pos) {
  pg_assert(pos < sb.len);

  if ('a' <= sb.data[pos] && sb.data[pos] <= 'z')
    sb.data[pos] -= 'a' - 'A';

  return sb;
}

__attribute__((warn_unused_result)) static Str_builder sb_clone(Str src,
                                                                Arena *arena) {
  Str_builder res = sb_new(src.len, arena);
  pg_assert(res.data);
  pg_assert(src.len <= res.cap);

  if (src.len) {
    pg_assert(src.data);
    memmove(res.data, src.data, src.len);
  }

  res.len = src.len;
  return res;
}

__attribute__((warn_unused_result)) static Str_builder
sb_replace_element_starting_at(Str_builder sb, usize start, u8 from, u8 to) {
  for (usize i = start; i < sb.len; i++) {
    pg_assert(sb.data);

    if (sb.data[i] == from)
      sb.data[i] = to;
  }
  return sb;
}

// ------------------- Utils

__attribute__((warn_unused_result)) static bool ut_char_is_alphabetic(u8 c) {
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z');
}

typedef struct {
  Str content;
  int error;
  pg_pad(4);
} Read_result;

__attribute__((warn_unused_result)) static Read_result
ut_read_all_from_fd(int fd, Str_builder sb) {
  pg_assert(fd > 0);

  while (sb_space(sb) > 0) {
    pg_assert(sb.len <= sb.cap);

    const i64 read_bytes = read(fd, sb_end_c(sb), sb_space(sb));
    if (read_bytes == -1)
      return (Read_result){.error = errno};
    if (read_bytes == 0)
      return (Read_result){.error = EINVAL}; // TODO: retry?

    sb = sb_assume_appended_n(sb, (usize)read_bytes);
    pg_assert(sb.len <= sb.cap);
  }
  return (Read_result){.content = sb_build(sb)};
}

__attribute__((warn_unused_result)) static char *str_to_c(Str s, Arena *arena) {
  char *c_str = arena_alloc(arena, sizeof(u8), _Alignof(u8), s.len + 1);
  if (s.data)
    memmove(c_str, s.data, s.len);

  pg_assert(strlen(c_str) == s.len);

  c_str[s.len] = 0;

  return c_str;
}

__attribute__((warn_unused_result)) static bool str_rcontains(Str haystack,
                                                              Str needle) {
  if (needle.len > haystack.len)
    return false;

  for (isize i = (isize)haystack.len - (isize)needle.len; i >= 0; i--) {
    const Str to_search = str_advance(haystack, (usize)i);

    if (str_starts_with(to_search, needle)) {
      return true;
    }
  }
  return false;
}

__attribute__((warn_unused_result)) static Read_result
ut_file_read_all(char *path, Arena *arena) {
  const int fd = open(path, O_RDONLY);
  if (fd == -1) {
    return (Read_result){.error = errno};
  }

  struct stat st = {0};
  if (stat(path, &st) == -1) {
    fprintf(stderr, "Failed to get the file size %s: %s\n", path,
            strerror(errno));
    close(fd);
    return (Read_result){.error = errno};
  }

  if (st.st_size == 0) {
    close(fd);
    return (Read_result){0};
  }

  Read_result res = ut_read_all_from_fd(fd, sb_new((usize)st.st_size, arena));
  close(fd);
  return res;
}

__attribute__((warn_unused_result)) static Read_result
ut_read_from_fd_until(int fd, Str_builder sb, Str needle) {
  pg_assert(fd > 0);

  bool found = false;
  while (sb_space(sb) > 0 && !(found = str_rcontains(sb_build(sb), needle))) {
    pg_assert(sb.len <= sb.cap);

    const i64 read_bytes = read(fd, sb_end_c(sb), sb_space(sb));
    if (read_bytes == -1)
      return (Read_result){.error = errno};
    if (read_bytes == 0)
      return (Read_result){.error = EINVAL}; // TODO: retry?

    sb = sb_assume_appended_n(sb, (usize)read_bytes);
    pg_assert(sb.len <= sb.cap);
  }

  return (found) ? (Read_result){.content = sb_build(sb)}
                 : (Read_result){.error = true};
}

__attribute__((warn_unused_result)) static Read_result
ut_file_mmap(char *path) {
  const int fd = open(path, O_RDONLY);
  if (fd == -1) {
    return (Read_result){.error = errno};
  }

  struct stat st = {0};
  if (stat(path, &st) == -1) {
    fprintf(stderr, "Failed to get the file size %s: %s\n", path,
            strerror(errno));
    close(fd);
    return (Read_result){.error = errno};
  }

  if (st.st_size == 0) {
    close(fd);
    return (Read_result){0};
  }

  const usize len = (usize)st.st_size;

  void *data = mmap(NULL, len, PROT_READ, MAP_PRIVATE | MAP_FILE, fd, 0);

  close(fd);

  if (data == NULL) {
    return (Read_result){.error = errno};
  }

  return (Read_result){.content = str_new(data, len)};
}

// --------------------------- Profile memory allocations

typedef struct {
  usize in_use_space, in_use_objects, alloc_space, alloc_objects;
  Array(usize) call_stack;
} Mem_record;
Array_struct(Mem_record);

struct Mem_profile {
  Array(Mem_record) records;
  usize in_use_space, in_use_objects, alloc_space, alloc_objects;
  Arena arena;
};

// TODO: Maybe use varints to reduce the size.
__attribute__((warn_unused_result)) static usize
ut_record_call_stack(usize *dst, usize cap) {
  usize *rbp = __builtin_frame_address(0);

  usize len = 0;

  while (rbp != 0 && ((usize)rbp & 7) == 0 && *rbp != 0) {
    const usize rip = *(rbp + 1);
    const usize caller_rsp = (usize)rbp + 16;
    const usize caller_rbp = *(usize *)rbp;

    // Check that rbp is within the right frame
    if (caller_rsp <= (usize)rbp || caller_rbp < caller_rsp)
      return len;

    rbp = (usize *)caller_rbp;

    // `rip` points to the return instruction in the caller, once this call is
    // done. But: We want the location of the call i.e. the `call xxx`
    // instruction, so we subtract one byte to point inside it, which is not
    // quite 'at it' but good enough.
    pg_assert(rip > 0);
    dst[len++] = rip - 1;

    if (len >= cap)
      return len;
  }
  return len;
}

static void mem_profile_record_alloc(Mem_profile *profile, usize objects_count,
                                     usize bytes_count) {
  // Record the call stack by stack walking.
  usize call_stack[64] = {0};
  usize call_stack_len = ut_record_call_stack(
      call_stack, sizeof(call_stack) / sizeof(call_stack[0]));

  // Update the sums.
  profile->alloc_objects += objects_count;
  profile->alloc_space += bytes_count;
  profile->in_use_objects += objects_count;
  profile->in_use_space += bytes_count;

  // Upsert the record.
  for (usize i = 0; i < profile->records.len; i++) {
    Mem_record *r = &profile->records.data[i];

    if (r->call_stack.len == call_stack_len &&
        memcmp(r->call_stack.data, call_stack,
               call_stack_len * sizeof(usize)) == 0) {
      // Found an existing record, update it.
      r->alloc_objects += objects_count;
      r->alloc_space += bytes_count;
      r->in_use_objects += objects_count;
      r->in_use_space += bytes_count;
      return;
    }
  }

  // Not found, insert a new record.
  Mem_record record = {
      .alloc_objects = objects_count,
      .alloc_space = bytes_count,
      .in_use_objects = objects_count,
      .in_use_space = bytes_count,
      .call_stack = array_make_from_slice(usize, call_stack,
                                          (u32)call_stack_len, &profile->arena),
  };

  *array_push(&profile->records, &profile->arena) = record;
}

static void mem_profile_write(Mem_profile *profile, FILE *out) {
  // clang-format off
  // heap profile:    <in_use>:  <nbytes1> [   <space>:  <nbytes2>] @ heapprofile
  // <in_use>: <nbytes1> [<space>: <nbytes2>] @ <rip1> <rip2> <rip3> [...]
  // <in_use>: <nbytes1> [<space>: <nbytes2>] @ <rip1> <rip2> <rip3> [...]
  // <in_use>: <nbytes1> [<space>: <nbytes2>] @ <rip1> <rip2> <rip3> [...]
  //
  // MAPPED_LIBRARIES:
  // [...]
  // clang-format on

  fprintf(out, "heap profile: %lu: %lu [     %lu:    %lu] @ heapprofile\n",
          profile->in_use_objects, profile->in_use_space,
          profile->alloc_objects, profile->alloc_space);

  for (usize i = 0; i < profile->records.len; i++) {
    Mem_record r = profile->records.data[i];

    fprintf(out, "%lu: %lu [%lu: %lu] @ ", r.in_use_objects, r.in_use_space,
            r.alloc_objects, r.alloc_space);

    for (usize j = 0; j < r.call_stack.len; j++) {
      fprintf(out, "%#lx ", r.call_stack.data[j]);
    }
    fputc('\n', out);
  }

  fputs("\nMAPPED_LIBRARIES:\n", out);

  static u8 mem[4096] = {0};
  int fd = open("/proc/self/maps", O_RDONLY);
  pg_assert(fd != -1);
  isize read_bytes = read(fd, mem, sizeof(mem));
  pg_assert(read_bytes != -1);
  close(fd);

  fwrite(mem, 1, (usize)read_bytes, out);

  fflush(out);
}

__attribute__((warn_unused_result)) static int ut_write_all(int fd, Str s) {
  Str remaining = s;
  while (!str_is_empty(remaining)) {
    ssize_t write_n = write(fd, remaining.data, remaining.len);
    if (write_n == -1)
      return errno;

    remaining = str_advance(remaining, (usize)write_n);
  }
  return 0;
}
