BEGIN {
  nasm_file = dest_dir"/asm_defines_nasm.h";
  gas_file =  dest_dir"/asm_defines_gas.h";
}

/@ASM_DEFINE/ {
  where = match($0, /offsetof_struct/);
  
  if(where != 0) {
    offset_name = $2;
    
    #remove any linefeed or carriage return character
    sub(/\r/, "", offset_name);
    sub(/\n/, "", offset_name);
    
    offset_value = $3;
    
    #remove any linefeed or carriage return character
    sub(/\r/, "", offset_value);
    sub(/\n/, "", offset_value);
    
    print "%define "offset_name" ("offset_value")" > nasm_file;
    print "#define "offset_name" ("offset_value")" > gas_file;
  }
}
END{}
