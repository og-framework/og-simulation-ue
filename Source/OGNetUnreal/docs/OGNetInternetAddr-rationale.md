<!-- SPDX-License-Identifier: MPL-2.0 -->
# `OGNetInternetAddr.h` and `.cpp` — rationale

`FInternetAddrOGNet` is the address the engine sees for a connection that runs over og-net's engine lane. This file is the
docs pointer of `OGNetInternetAddr.h` and `OGNetInternetAddr.cpp`. The source holds the licence, a docs pointer and code; it
states no prohibition, so there is no guards file.

Engine source citations are to Unreal Engine 5.6 and are written without backticks, because the engine is outside every
tree this document's lint can resolve against.

**If this file and the source disagree, the source is authoritative and this file is stale.**

---

## §1 What it holds, and why it holds no IP

The class holds one value, the og-net connection id (`Id`). og-net identifies a connection by that id, never by the
peer's address: a peer may roam to a new address and keep its connection, and og-net's ids are non-zero and unique among
an endpoint's live connections. So the engine's notion of "the remote address" of an OG connection is the id, and nothing
the engine stores about a connection (its map key, its log text, its URL host) changes when the peer roams.

## §2 Equality and hash

- `operator==` is true only for another `FInternetAddrOGNet` with the same id. The base class's default compares the raw
  bytes and the port (IPAddress.h, the operator== of FInternetAddr), so without the protocol check a platform IPv4 address
  whose four bytes happened to spell an id would compare equal to it.
- `GetTypeHash` is the id.
- Both matter to the engine's connection map: `AddClientConnection` keys MappedClientConnections by the connection's remote
  address (NetDriver.cpp, lines 6651-6655), and that map hashes with GetTypeHash and matches with operator== (IPAddress.h,
  FInternetAddrConstKeyMapFuncs, lines 220-236).
- A C++20 caller comparing two `FInternetAddrOGNet` values directly with `==` gets an ambiguity error (the reversed
  candidate of the base-class operator ties with the forward one); compare through `const FInternetAddr&`, as the engine
  always does and as the test's `AddressesEqual` does.

## §3 The rest of the interface

- `ToString` is `OGNet:` and the id as 8 lowercase hex digits, with or without `bAppendPort`: there is no port.
- `GetProtocolType` is the name `OGNet`, built once.
- `IsValid` is `Id != 0`. og-net never gives a live connection the id 0; the client's server connection carries id 0 until
  og-net's connection is up (`OGNetConnection-rationale.md` §2).
- `GetPort` is 0. The IP and port setters do nothing, and `SetIp` from text reports the text invalid: an id cannot be set
  from an IP.
- `GetRawIp` returns the id's four bytes, most significant first, and `GetIp` returns the id itself, so engine code that
  reads raw bytes sees the id rather than an empty array.
- `Clone` is a new `FInternetAddrOGNet` with the same id.

## §4 The test `OGNet.Unreal.Driver.InternetAddr`

The automation test (EditorContext, ProductFilter, in `OGNetDriverTests.cpp`) checks equality and inequality, the hash,
`ToString` with and without the port and its zero padding, the protocol name, validity of a non-zero and of a zero id,
`Clone` both ways, `GetRawIp` byte by byte, `GetIp`, that the setters leave the id and the port alone, and that the address
never equals a platform address built from the same four bytes.
