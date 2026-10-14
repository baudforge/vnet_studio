# Runtime logs

Select **Logs** in the sidebar, use **Ctrl + 2** (Command + 2 on macOS), or select **View > Logs**.

The log table shows the time, level, source, and message for workspace startup, application launches, docking, closing, preference changes, and failed workspace actions. Renderer warnings and errors identify the workspace or application instance that produced them.

## Find a message

Enter text in **Search logs** to filter by source or message. Select **All levels**, **Info**, **Warning**, or **Error** to filter by severity. Text search and severity filtering work together.

Entries appear in chronological order. The timestamp tooltip shows the full date and time. Log text is read-only and can be selected and copied.

## Clear and retention

Select **Clear** to remove the current entries. New records appear as activity continues.

Logs keep the most recent 500 entries in memory for the current program session. Older entries are discarded. Closing and reopening the workspace while an application window remains open retains the logs. Quitting VNet Studio clears them.

Opening Logs keeps application instances running in the background. Return to [Home](instances.md) to work with them.
