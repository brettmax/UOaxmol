# Server packaging

Axmol is a client engine; the server does not use it. ModernUO and the project's server-side
repositories (UOScripts, UOModernSpawner, UOIORingGroup, UOSerializationGenerator) stay on
.NET and are packaged separately. The client speaks the standard UO protocol, so any ModernUO
shard works as the backend.

The protocol constants the two sides must agree on (packet lengths per client version, the
Huffman table) live in `uo/core/net` and are covered by `uo/tests`.
