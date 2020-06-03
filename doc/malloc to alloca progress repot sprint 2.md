## Malloc to alloca

by Alfiya Mussabekova

### Development state

From last sprint there left unsolved assertion fail “Assertion New->getType() == getType() && replaceAllUses of value with new value with different type”, to fix it, we decided to allocate array of *i8*, because return type of malloc is *i8**.
However, while implementing we changed 'array of *i8*' to 'many *i8*' and added checking on how much bytes are allocated, for now we decided that it is safe to allocate 256 bytes.

Optimization is completed and works in my teammate's computer, but we did not have enough time to test it. Most probably in my computer there is environment problem, because of which I get another assertion fail: "isValidArgumentType(Params[i]) && "Not a valid type for function argument!"

### Future plan

- Fix environment problem
- Complete testing
- Fix potential bugs





