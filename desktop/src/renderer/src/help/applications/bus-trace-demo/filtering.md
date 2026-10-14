# Filtering messages

Enter text in the filter field below the Message trace toolbar. Matching is case-insensitive.

## Search fields

The filter matches the message ID, message name, and payload. Clearing the field restores all buffered frames.

Examples:

```text
0x123         Match a message identifier
wheel speed   Match a message name
11 2E         Match consecutive payload bytes
```

## Matching rows

The count beside Message trace reflects the matching rows. A filter changes the displayed rows without deleting messages from the buffer.

If no rows match, adjust the query or clear the filter. Generation and [buffer limits](message-trace.md#buffer-size) continue to apply.

## Instance state

Each instance retains its own filter when switching tabs, moving to an independent window, or docking back to Home.
