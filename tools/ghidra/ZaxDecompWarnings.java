// Decompiles every function of the program and writes the decompiler's warnings per function:
// <out>/decomp_warnings.tsv: address, name, kind, detail (one row per warning line; kind "FAILED" when the decompile did not complete)
// <out>/decomp_c/<address>.c: the C text of every function with at least one warning.
// Argument: output folder.
// @category Zax
import ghidra.app.decompiler.*;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import java.io.*;
import java.nio.file.*;

public class ZaxDecompWarnings extends GhidraScript {
    @Override
    public void run() throws Exception {
        Path out = Paths.get(getScriptArgs()[0]);
        Files.createDirectories(out.resolve("decomp_c"));
        DecompInterface ifc = new DecompInterface();
        ifc.toggleCCode(true);
        ifc.openProgram(currentProgram);
        int n = 0, hit = 0;
        try (PrintWriter w = new PrintWriter(Files.newBufferedWriter(out.resolve("decomp_warnings.tsv")))) {
            w.println("address\tname\tkind\tdetail");
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                if (f.isExternal() || f.isThunk()) continue;
                n++;
                String addr = "0x" + f.getEntryPoint().toString();
                String name = f.getName(true);
                DecompileResults r = ifc.decompileFunction(f, 120, monitor);
                if (r == null || !r.decompileCompleted() || r.getDecompiledFunction() == null) {
                    String msg = r == null ? "null" : r.getErrorMessage();
                    w.printf("%s\t%s\tFAILED\t%s%n", addr, name, msg == null ? "" : msg.replace('\n', ' ').replace('\t', ' '));
                    hit++;
                    continue;
                }
                String c = r.getDecompiledFunction().getC();
                boolean any = false;
                for (String line : c.split("\n")) {
                    String t = line.trim();
                    if (t.contains("WARNING") || t.contains("halt_baddata") || t.contains("Bad instruction") || t.contains("Could not recover jumptable")) {
                        String kind = t.contains("Removing unreachable block") ? "UNREACHABLE"
                                : t.contains("Could not recover jumptable") ? "JUMPTABLE"
                                : t.contains("halt_baddata") ? "BADDATA"
                                : t.contains("Bad instruction") ? "BADINSN" : "WARNING";
                        w.printf("%s\t%s\t%s\t%s%n", addr, name, kind, t.replace('\t', ' '));
                        any = true;
                    }
                }
                if (any) {
                    hit++;
                    Files.writeString(out.resolve("decomp_c/" + addr + ".c"), c);
                }
                if (n % 500 == 0) println("ZaxDecompWarnings " + n + " functions, " + hit + " with warnings");
            }
        }
        ifc.dispose();
        println("ZaxDecompWarnings done: " + n + " functions, " + hit + " with warnings");
    }
}
