# libgdata

libgdata is a GLib-based library for accessing online service APIs using the
GData protocol — most notably, Google's services. It provides APIs to access
the common Google services, and has full asynchronous support.

See the test programs in `gdata/tests/` for simple examples of how to use the
code.

libgdata is API and ABI stable.

## Dependencies

 * glib-2.0 ≥ 2.44.0
 * libxml-2.0
 * gio-2.0 ≥ 2.44.0
 * libsoup-2.4 ≥ 2.42.0
 * liboauth ≥ 0.9.4
 * json-glib ≥ 0.15.0

If compiling with `--enable-gnome` (for GNOME support):
 * gcr-base-3
 * goa-1.0 ≥ 3.8

If compiling the demos:
 * gtk+-3.0 ≥ 2.91.2

## Environment variables

If the environment variable `LIBGDATA_DEBUG` is set to one of the following
values, libgdata will give debug output (at various levels):
 * `0`: Output no debug messages or network logs
 * `1`: Output debug messages, but not network logs
 * `2`: Output debug messages and network traffic headers
 * `3`: Output debug messages and full network traffic logs, redacting usernames,
   passwords and auth. tokens
 * `4`: Output debug messages and full network traffic logs, and don't redact
   usernames, passwords and auth. tokens

If `LIBGDATA_DEBUG` is unset, no debug output will be produced.

So, to debug a program which uses libgdata, run it from a terminal with the
following command:

```
LIBGDATA_DEBUG=3 ./my-program-name &> libgdata.log
```

## Deprecation guards

If `LIBGDATA_DISABLE_DEPRECATED` is defined when compiling against libgdata, all
deprecated API will be removed from included headers.

## Default branch renamed to `main`

The default development branch of libgdata has been renamed to `main`. To update
your local checkout, use:
```sh
git checkout master
git branch -m master main
git fetch
git branch --unset-upstream
git branch -u origin/main
git symbolic-ref refs/remotes/origin/HEAD refs/remotes/origin/main
```

## Licensing

libgdata is licensed under the LGPL; see COPYING for more details.

## Contact

https://wiki.gnome.org/Projects/libgdata
