# Alt Manager test plan

- [ ] Build Loader and DLL for Release | x64; record command, exit code, and output.
- [ ] On Minecraft 1.8.9, apply a valid username and confirm the local session name updates.
- [ ] Reject names outside `[A-Za-z0-9_]{3,16}` without changing the current session.
- [ ] Restore the original session and confirm its username returns.
- [ ] Confirm DLL shutdown restores the original session.
- [ ] Exercise at least one supported named/SRG client and one obfuscated client; record the exact client and result.
