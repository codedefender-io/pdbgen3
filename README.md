This tool is used to generate a new PDB file for a PE file obfuscated by CodeDefender.

## Example

Here is an example on how you use `pdbgen3` to generate a PDB file for an obfuscated CodeDefender binary.

```
pdbgen3.exe --obf-pe=example/HelloWorld.obfuscated.exe --debug-file=example/HelloWorld.dbg --orig-pdb=example/HelloWorld.pdb --out-pdb=example/HelloWorld.obfuscated.pdb
```

This will generate a new pdb `HelloWorld.obfuscated.pdb` in the `example/` folder.

## Precompiled

You can download a pre-compiled version of this project instead of having to build this entire project. Head over to the github releases tab.

## Building

This will generate cmake `build` folder. You can then go into `build/` and open `pdbgen2.sln`. You need to have `Visual Studios 2022` installed. It will take upwards of 30 minutes to configure, build, and install llvm.

```
cmake -B build -DLLVM_BUILD_TYPE=Release
# or
cmake -B build -DLLVM_BUILD_TYPE=Debug
```

Delete the `build/` folder if you wish to switch between `Release` and `Debug`, then re-run the above command.

## Debug File Format

This is just a binary file that contains the following format:

```
(start rva, end rva, orig rva)
```

The number of entries is equal to the size of omap file divided by `0xC`.

## Credits

- [PdbGen](https://github.com/gix/PdbGen)
- [llvm-docs](https://llvm.org/docs/PDB/index.html)
- https://www.youtube.com/watch?v=gxmXWXUvNr8