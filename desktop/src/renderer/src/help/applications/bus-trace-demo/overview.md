# Bus Trace Demo

Bus Trace Demo displays simulated Classic CAN messages. It does not connect to CAN hardware or send messages to a vehicle.

## Start the demo

1. Open Apps and launch **Bus Trace Demo**.
2. Select **Start demo** in the Message trace toolbar.
3. Select **Pause** to stop generation, or **Resume demo** to continue.

The simulation generates two messages per second. Each instance has its own trace buffer, filter, and playback state.

## Read the trace

The table shows the newest messages first. It keeps up to 120 frames; older frames are discarded as new messages arrive.

See [Message trace](message-trace.md) for the columns and controls, or [Filtering messages](filtering.md) for searching the trace.

## Tabs and windows

The demo can move between a Home tab and an independent window without resetting its trace. Use the [main program's window controls](../../workspace/instances.md).
