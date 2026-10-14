# Message trace

Select **Start demo** to begin generating messages. The Message trace count shows the number of rows matching the current filter.

## Columns

| Column     | Meaning                                  |
| ---------- | ---------------------------------------- |
| #          | Sequence number within this instance.    |
| Time       | Local generation time with milliseconds. |
| Message ID | Simulated CAN identifier in hexadecimal. |
| Name       | Simulated message name.                  |
| DLC        | Payload length, currently 8 bytes.       |
| Data       | Hexadecimal payload bytes.               |

## Pause and resume

**Pause** stops generating new messages. Existing rows and the filter remain available. **Resume demo** continues the same instance's sequence.

The simulation continues while its Home tab is hidden or another workspace page is selected.

## Clear the trace

**Clear** empties the buffer and resets the sequence number. If generation is running, new messages continue to arrive.

## Buffer size

Only the newest 120 frames are kept in memory. The table scrolls within the application page.

For finding particular messages, see [Filtering messages](filtering.md).
