BEGIN {
  nasm_file = dest_dir"/asm_defines_nasm.h";
  gas_file =  dest_dir"/asm_defines_gas.h";
}

{
  where = match($0, /offsetof_struct/);
  
  if(where != 0) {
    offset_name = substr($0, RSTART);
    
    #remove any linefeed or carriage return character
    sub(/\r/, "", offset_name);
    sub(/\n/, "", offset_name);
    
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
