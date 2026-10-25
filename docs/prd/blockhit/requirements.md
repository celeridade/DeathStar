# BlockHit requirements

- [ ] Add a BlockHit toggle to the loader; hide its mode and delay controls while disabled.
- [ ] Manual mode repeatedly emits right-button clicks while the physical right mouse button is held.
- [ ] Automatic mode repeatedly emits right-button clicks while the physical right mouse button is held, using a configurable 0-150 ms interval.
- [ ] Show the delay control only in Automatic mode; Manual mode uses its fixed interval.
- [ ] Stop emitting clicks immediately when the button is released, the module is disabled, focus is lost, or a GUI opens.
- [ ] Persist the enabled state, mode, and automatic interval in the loader configuration.
