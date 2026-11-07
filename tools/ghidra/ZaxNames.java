// Imports names and class structures into the Zax project.
// Argument: repository root (zax-editor). Reads out/ghidra/names.tsv, docs/classes.csv, docs/fields.csv.
// Run via tools/ghidra.sh.
// @category Zax
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.*;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.nio.file.*;
import java.util.*;

public class ZaxNames extends GhidraScript {

    Namespace ns(String name) throws Exception {
        if (name.isEmpty()) return currentProgram.getGlobalNamespace();
        SymbolTable st = currentProgram.getSymbolTable();
        Namespace n = st.getNamespace(name, currentProgram.getGlobalNamespace());
        if (n == null) n = st.createClass(currentProgram.getGlobalNamespace(), name, SourceType.IMPORTED);
        return n;
    }

    @Override
    public void run() throws Exception {
        Path root = Paths.get(getScriptArgs()[0]);
        int funcs = 0, labels = 0, created = 0;
        for (String line : Files.readAllLines(root.resolve("out/ghidra/names.tsv"))) {
            String[] f = line.split("\t", -1);
            Address a = toAddr(f[0]);
            Namespace n = ns(f[1]);
            if (f[3].equals("function")) {
                Function fn = getFunctionAt(a);
                if (fn == null) {
                    disassemble(a);
                    fn = createFunction(a, null);
                    if (fn == null) { println("no function at " + f[0]); continue; }
                    created++;
                }
                fn.setParentNamespace(n);
                fn.setName(f[2], SourceType.IMPORTED);
                funcs++;
            } else {
                Symbol s = currentProgram.getSymbolTable().createLabel(a, f[2], n, SourceType.IMPORTED);
                s.setPrimary();
                labels++;
            }
        }
        println("Functions named: " + funcs + " (newly created " + created + "), labels: " + labels);

        // Class structures: vtable pointer at 0, registered fields (including inherited ones) at their offsets
        Map<String, String[]> cls = new LinkedHashMap<>();
        List<String> kl = Files.readAllLines(root.resolve("docs/classes.csv"));
        List<String> head = Arrays.asList(kl.get(0).split(","));
        for (String l : kl.subList(1, kl.size())) cls.put(l.split(",")[0], l.split(",", -1));
        Map<String, List<String[]>> fields = new HashMap<>();
        List<String> fl = Files.readAllLines(root.resolve("docs/fields.csv"));
        for (String l : fl.subList(1, fl.size())) {
            String[] f = l.split(",", -1);
            fields.computeIfAbsent(f[0], k -> new ArrayList<>()).add(f);
        }
        int iP = head.indexOf("parent"), iS = head.indexOf("size");
        DataTypeManager dtm = currentProgram.getDataTypeManager();
        CategoryPath cat = new CategoryPath("/Zax");
        int tx = dtm.startTransaction("Zax classes");
        int nstruct = 0;
        try {
            for (String name : cls.keySet()) {
                int size = Integer.parseInt(cls.get(name)[iS]);
                StructureDataType s = new StructureDataType(cat, name, size, dtm);
                s.replaceAtOffset(0, new PointerDataType(VoidDataType.dataType), 4, "vftable", null);
                List<String> chain = new ArrayList<>();
                for (String c = name; c != null && !c.isEmpty(); c = cls.containsKey(c) ? cls.get(c)[iP] : null) chain.add(0, c);
                for (String c : chain) {
                    for (String[] f : fields.getOrDefault(c, List.of())) {
                        // columns: class,index,name,path,encoding,offset,size,bitmask
                        if (f[5].isEmpty() || f[6].isEmpty()) continue;
                        int off = Integer.decode(f[5]), len = Integer.parseInt(f[6]);
                        if (f.length > 7 && !f[7].isEmpty()) {
                            // switches: several fields share one word, told apart by bit mask
                            if (off > 0 && off + 4 <= size) {
                                DataTypeComponent d = s.getComponentAt(off);
                                String note = (d != null && d.getFieldName() != null && d.getFieldName().startsWith("flags_")
                                        && d.getComment() != null ? d.getComment() + ", " : "") + f[2] + "=" + f[7];
                                try {
                                    s.replaceAtOffset(off, UnsignedIntegerDataType.dataType, 4, String.format("flags_%x", off), note);
                                } catch (Exception e) { /* overlaps another field */ }
                            }
                            continue;
                        }
                        if (off <= 0 || len <= 0 || off + len > size) continue;
                        DataType t = switch (f[4]) {
                            case "i4" -> len == 4 ? IntegerDataType.dataType : null;
                            case "f4" -> len == 4 ? FloatDataType.dataType : null;
                            case "f8" -> len == 8 ? DoubleDataType.dataType : null;
                            case "u8" -> len == 1 ? ByteDataType.dataType : null;
                            case "ptr" -> len == 4 ? new PointerDataType(VoidDataType.dataType) : null;
                            default -> null;
                        };
                        if (t == null) t = new ArrayDataType(ByteDataType.dataType, len, 1);
                        try {
                            s.replaceAtOffset(off, t, len, f[2].replaceAll("[^A-Za-z0-9_]", "_"), c + ": " + f[3]);
                        } catch (Exception e) { /* skip overlapping fields */ }
                    }
                }
                dtm.addDataType(s, DataTypeConflictHandler.REPLACE_HANDLER);
                nstruct++;
            }
        } finally {
            dtm.endTransaction(tx, true);
        }
        println("Class structures: " + nstruct);
    }
}
