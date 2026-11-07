// Writes all functions to out/ghidra/functions_raw.tsv:
// address, size (bytes in the body), namespace, name, thunk, callers, callees.
// Argument: repository root (zax-editor).
// @category Zax
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import java.io.*;
import java.nio.file.*;

public class ZaxFunctions extends GhidraScript {
    @Override
    public void run() throws Exception {
        Path out = Paths.get(getScriptArgs()[0]).resolve("out/ghidra/functions_raw.tsv");
        int n = 0;
        try (PrintWriter w = new PrintWriter(Files.newBufferedWriter(out))) {
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                if (f.isExternal()) continue;
                String ns = f.getParentNamespace().isGlobal() ? "" : f.getParentNamespace().getName(true);
                w.printf("0x%s\t%d\t%s\t%s\t%d\t%d\t%d%n", f.getEntryPoint().toString(),
                        f.getBody().getNumAddresses(), ns, f.getName(), f.isThunk() ? 1 : 0,
                        f.getCallingFunctions(monitor).size(), f.getCalledFunctions(monitor).size());
                n++;
            }
        }
        println("Functions exported: " + n);
    }
}
