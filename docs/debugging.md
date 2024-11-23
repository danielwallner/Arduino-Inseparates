# Debugging and Testing

---

## Debugging

Debugging and developing multi-tasking systems can be challenging.<br/>
Too make this somewhat easier it is possible to run and debug parts of this library in an IDE on host.

The test code contains "debug_*" targets that can run with mockups of Arduino functionality.

To setup debugging on MacOS:

```bash
cd test
mkdir debug
cd debug
cmake -GXcode ..
open InseparatesTest.xcodeproj
```

Then run one of the debug_* targets.

## Running Tests

1. **Installation**: Make sure you have [CMake](https://cmake.org) installed.
2. **Run Tests**:

   ```bash
   cd test
   ./run_tests.sh
   ```

**[Back to Main Documentation](../README.md)**
