#pragma once
// CGI cannot keep long-lived sockets (WebSockets). Stubs provided for completeness.
void socket_push_bid_update(long auction_id, double amount); // no-op
