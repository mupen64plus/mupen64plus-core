BEGIN {
  nasm_file = "../../src/main/asm_defines_nasm.h";
  gas_file = "../../src/main/asm_defines_gas.h";
}

NR==1 {
  msvc = match($0, /Microsoft/);
}

{
  where = match($0, /offsetof_struct/);
  
  if(where != 0) {
    offset_name = substr($0, RSTART);
    
    #remove any linefeed or carriage return character
    sub(/\r/, "", offset_name);
    sub(/\n/, "", offset_name);
    
    if(msvc != 0)
      offset_value = $2;
    else
      offset_value = $1;
        
    sub(/0x/, "", offset_value);
    
    #remove any linefeed or carriage return character
    sub(/\r/, "", offset_value);
    sub(/\n/, "", offset_value);
    
    print "%define "offset_name" (0x"offset_value"-1)" > nasm_file;
    print "#define "offset_name" (0x"offset_value"-1)" > gas_file;
  }
}
END{}
