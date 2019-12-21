//Haoyue Cui (hac113)
import java.io.*; 
import java.util.*; 
import java.lang.Math; 
public class vmsim{
  //always 4KB page size
  public static boolean need = false;
  public static void main(String[] args){

    if(args.length < 5 || args.length > 7 || args.length == 6){
      System.out.println("Usage: vmsim"  + " –n <numframes> -a <opt|fifo|aging> [-r <refresh>] <tracefile>");
      System.exit(0);
    }

    int numframes = Integer.parseInt(args[1]);
    String algorithm = args[3];
    String filename = "";
    int refresh_parameter = 0;
    if(args.length == 5){
      filename = args[4];
    }else{
      filename = args[6];
      refresh_parameter = Integer.parseInt(args[5]);
    }
    if(args[3].equals("fifo")) 
      fifo(filename, numframes);
    else if(args[3].equals("opt")) 
      opt(filename, numframes);
    else if(args[3].equals("aging")){
      if(!args[4].equals("-r")){
        System.out.println("Need refresh parameter");
        System.exit(0);
      }
      aging(filename, numframes, refresh_parameter);    
    }
}

public static void fifo(String filename, int numframes){
  int numfaults = 0;
  int numwrites = 0;
  int page2evict = 0;

  int numaccesses = 0;
      
  List<Character> mode_array = new ArrayList<Character>();
  List<Integer> address_array = new ArrayList<Integer>();

 
  File f = new File(filename);
    Scanner sc = new Scanner(System.in);
    try{sc = new Scanner(f);}
    catch(Exception e){
      System.out.println("File not found.\n");
      System.exit(0);
    }
    PTE[] RAM = new PTE[numframes];
      for(int i = 0; i < RAM.length; i++){
        PTE temp = new PTE();
        temp.set_frame(i);
        RAM[i] = temp;
      }
    //Initialize page table
    PTE[] pageTable = new PTE[(int)Math.pow(2,20)];
    for(int i = 0 ; i < pageTable.length ; i++)
    {
      PTE temp = new PTE();
      temp.set_address(i);
      pageTable[i] = temp;
    }
    for(int i = 0; sc.hasNextLine(); i++){
      numaccesses++;
      String[] eachline = sc.nextLine().split(" ");
      //read load or store from trace file
      mode_array.add(eachline[0].charAt(0));
      //read page number from trace file
      address_array.add(Integer.decode(eachline[1].substring(0,7)));
    }

    //Main loop to process memory access
    for(int i = 0; i<numaccesses; i++){
      Integer page_num = address_array.get(i);
      int mode_type = mode_array.get(i);

      //check if page is in bound
      if(page_num<0||page_num>=pageTable.length){
        System.out.println("Page not in bound.\n");
        System.exit(0);
      }

      if(pageTable[page_num].is_exist()){ //address already in RAM
        if(mode_array.get(i)=='s'){
          RAM[pageTable[page_num].get_frame()].set_dirty(1);
        }
        // System.out.println("Not page fault - hit");
      }else{
        if(RAM[page2evict].get_address() == -1){ //if address is not yet valid, initialize
          pageTable[page_num].set_exist(true);
          pageTable[page_num].set_frame(page2evict);
          if(mode_array.get(i) == 's')
            pageTable[page_num].set_dirty(1);
          RAM[page2evict] = pageTable[page_num];
          // System.out.println("page fault - no eviction");
        }
        else{
          //evict - first in first out
          if(RAM[page2evict].is_dirty() == 1){
              numwrites++;
              // System.out.println("page fault - evict dirty");
          }
          else{
              // System.out.println("page fault - evict clean");
          }

          //evict: set evict PTE back to default value
          int temp_address = RAM[page2evict].get_address();
          pageTable[temp_address].set_dirty(0);
          pageTable[temp_address].set_exist(false);
          pageTable[temp_address].set_frame(-1);

          //initialize the new page adding in RAM
          pageTable[page_num].set_exist(true);
          pageTable[page_num].set_frame(page2evict);
          if(mode_array.get(i) == 's')
            pageTable[page_num].set_dirty(1);
          RAM[page2evict] = pageTable[page_num];
        }
      numfaults++;
      page2evict = (page2evict + 1)%numframes;
      }
    }
    output("FIFO",numframes, numaccesses,numfaults,numwrites);
}

public static void opt(String filename, int numframes){
  int numfaults = 0;
  int numwrites = 0;
  int page2evict = 0;

  int numaccesses = 0;
      
  List<Character> mode_array = new ArrayList<Character>();
  List<Integer> address_array = new ArrayList<Integer>();
 
  File f = new File(filename);
    Scanner sc = new Scanner(System.in);
    try{sc = new Scanner(f);}
    catch(Exception e){
      System.out.println("File not found.\n");
      System.exit(0);
    }
    PTE[] RAM = new PTE[numframes];
      for(int i = 0; i < RAM.length; i++){
        PTE temp = new PTE();
        temp.set_frame(i);
        RAM[i] = temp;
      }
    //Initialize page table
    PTE[] pageTable = new PTE[(int)Math.pow(2,20)];
    for(int i = 0 ; i < pageTable.length ; i++){
      PTE temp = new PTE();
      temp.set_address(i);
      pageTable[i] = temp;
    }
    for(int i = 0; sc.hasNextLine(); i++){
      numaccesses++;
      String[] eachline = sc.nextLine().split(" ");
      mode_array.add(eachline[0].charAt(0));
      address_array.add(Integer.decode(eachline[1].substring(0,7)));
    }
    //array to keep track of use and initialize
    int[] least_recent_use = new int[numframes];
    for(int o = 0; o<numframes; o++){
      least_recent_use[o] = 0;
    }

    //Main loop to process memory access
    for(int i = 0; i<numaccesses; i++){
      Integer page_num = address_array.get(i);
      int mode_type = mode_array.get(i);

      if(page_num<0||page_num>=pageTable.length){
        System.out.println("Page not in bound.\n");
        System.exit(0);
      }
      if(pageTable[page_num].is_exist()){
        //no page fault-hit: recent use++
        least_recent_use[pageTable[page_num].get_frame()]++;
        if(mode_array.get(i)=='s'){
          RAM[pageTable[page_num].get_frame()].set_dirty(1);
        }
        // System.out.println("Not page fault - hit");
      }else{
        if(RAM[page2evict].get_address() == -1){
          //RAM not full: recent use++
          pageTable[page_num].set_exist(true);
          pageTable[page_num].set_frame(page2evict);
          least_recent_use[pageTable[page_num].get_frame()]++;
          if(mode_array.get(i) == 's')
            pageTable[page_num].set_dirty(1);
          RAM[page2evict] = pageTable[page_num];
          // System.out.println("page fault - no eviction");
        }
        else{
          //have to evict page - no enough room in RAM
          int[] needed_array = new int[numframes];
          for(int m = 0; m < numframes; m++){
            needed_array[m] = -1;
          }
          boolean foundnext = false;
          boolean found_evict = false;
          for(int k = 0; k < numframes; k++){
            foundnext = false;
            found_evict = false;
            Integer needed_address = RAM[k].get_address();
            // System.out.println("needed address: "+ needed_address);
            for(int location = i+1; location<numaccesses; location++){
              if(address_array.get(location).equals(needed_address)){
                needed_array[k] = location - i-1;
                foundnext = true;
                break;
              }
            }
            if(!foundnext){
              page2evict = k;
              found_evict = true;
              break;
            }
          }
          if(!found_evict){
            int evict = -1;
            for(int j = 0; j<numframes; j++){
              if (needed_array[j] > evict){
                evict = needed_array[j];
                page2evict = j;
              }else if(needed_array[j] == evict){
                if(least_recent_use[j] > evict){
                  evict = needed_array[j];
                  page2evict = j;
                }
              }
            }
          }
          if(RAM[page2evict].is_dirty()==1){
              numwrites++;
          }
          //evict: set PTE to default value
          int temp_address = RAM[page2evict].get_address();
          pageTable[temp_address].set_dirty(0);
          pageTable[temp_address].set_exist(false);
          pageTable[temp_address].set_frame(-1);
          least_recent_use[page2evict] = 0;


          pageTable[page_num].set_exist(true);
          pageTable[page_num].set_frame(page2evict);
          least_recent_use[page2evict]++;
          if(mode_array.get(i) == 's')
            pageTable[page_num].set_dirty(1);
          RAM[page2evict] = pageTable[page_num];
        }
      numfaults++;
      page2evict = (page2evict + 1)%numframes;
      }
    }
  output("OPT", numframes, numaccesses,numfaults,numwrites);
}

public static void aging(String filename, int numframes, int refresh_parameter){
  int numfaults = 0;
  int numwrites = 0;
  int page2evict = 0;

  int numaccesses = 0;
      
  List<Character> mode_array = new ArrayList<Character>();
  List<Integer> address_array = new ArrayList<Integer>();
  List<Integer> cycles_array = new ArrayList<Integer>();
 
  File f = new File(filename);
    Scanner sc = new Scanner(System.in);
    try{sc = new Scanner(f);}
    catch(Exception e)
    {
      System.out.println("File not found.\n");
      System.exit(0);
    }
    PTE[] RAM = new PTE[numframes];
      for(int i = 0; i < RAM.length; i++){
        PTE temp = new PTE();
        temp.set_frame(i);
        RAM[i] = temp;
      }
    //Initialize page table
    PTE[] pageTable = new PTE[(int)Math.pow(2,20)];
    for(int i = 0 ; i < pageTable.length ; i++)
    {
      PTE temp = new PTE();
      temp.set_address(i);
      pageTable[i] = temp;
    }
    for(int i = 0; sc.hasNextLine(); i++){
      numaccesses++;
      String[] eachline = sc.nextLine().split(" ");
      mode_array.add(eachline[0].charAt(0));
      address_array.add(Integer.decode(eachline[1].substring(0,7)));
      cycles_array.add(Integer.parseInt(eachline[2]));
    }
    // System.out.println(numaccesses);
    int[] counter_array = new int[numframes];
    for(int r = 0; r<numframes; r++){
      counter_array[r] = 0;
    }
    int last_cycle_left = 0;
    int countersize = 0;

    //Main loop to process memory access
    for(int i = 0; i<numaccesses; i++){
      int page_num = address_array.get(i);
      int mode_type = mode_array.get(i);

      if(page_num<0||page_num>=pageTable.length){
        System.out.println("Page not in bound.\n");
        System.exit(0);
      }

      if(pageTable[page_num].is_exist()){
        //check refresh
        last_cycle_left = check_refresh(numframes,RAM,countersize, page2evict, counter_array, cycles_array, i, refresh_parameter, last_cycle_left);
        //set refrence to 1 since it exists in RAM already
        RAM[pageTable[page_num].get_frame()].set_referenced(1);
        if(mode_array.get(i)=='s'){
          RAM[pageTable[page_num].get_frame()].set_dirty(1);
        }
        // System.out.println("Not page fault - hit");
      }else{
        if(RAM[page2evict].get_address() == -1){
          //check refresh
          last_cycle_left = check_refresh(numframes,RAM,countersize, page2evict, counter_array, cycles_array, i, refresh_parameter, last_cycle_left);
          //initialize - set counter 0x10000000
          counter_array[page2evict] = 128;
          pageTable[page_num].set_exist(true);
          pageTable[page_num].set_frame(page2evict);
          pageTable[page_num].set_referenced(0);
          if(mode_array.get(i) == 's')
            pageTable[page_num].set_dirty(1);
          RAM[page2evict] = pageTable[page_num];
          countersize++;
          // System.out.println("page fault - no eviction");
        }
        else{
          //check refresh
          last_cycle_left = check_refresh(numframes,RAM, countersize, page2evict, counter_array, cycles_array, i, refresh_parameter, last_cycle_left);
          //check a evict page
          int possible_evict_page = 0;
          //loop for all frames
          for(int l = 0; l < numframes; l++){
              if(counter_array[l] < counter_array[possible_evict_page]){ //if counter small, change possible_evict
                possible_evict_page = l;
              }else if(counter_array[l] == counter_array[possible_evict_page]){ //if equal, compare dirty setting
                if(RAM[l].is_dirty() < RAM[possible_evict_page].is_dirty()){ //if dirty small, change possible_evict
                  possible_evict_page = l;
                }else if(RAM[l].is_dirty() == RAM[possible_evict_page].is_dirty()){//if equal, compare virtual page number
                  if(RAM[l].get_address() < RAM[possible_evict_page].get_address()){//pick the smaller virtual page number
                    possible_evict_page = l;
                  }
                }
              }
            }
            page2evict = possible_evict_page;    
          //evict setting
          if(RAM[page2evict].is_dirty()==1){
              numwrites++;
              // System.out.println("page fault - evict dirty");
          }
          else{
              // System.out.println("page fault - evict clean");
          }
          
          //evict: set PTE to default value
          int temp_address = RAM[page2evict].get_address();
          pageTable[temp_address].set_dirty(0);
          pageTable[temp_address].set_exist(false);
          pageTable[temp_address].set_frame(-1);
          pageTable[temp_address].set_referenced(-1);


          pageTable[page_num].set_exist(true);
          pageTable[page_num].set_frame(page2evict);
          pageTable[page_num].set_referenced(0);
          counter_array[page2evict] = 128;
          if(mode_array.get(i) == 's')
            pageTable[page_num].set_dirty(1);
          RAM[page2evict] = pageTable[page_num];
        }
      numfaults++;
      page2evict = (page2evict + 1)%numframes;
      }
    }
  output("AGING", numframes, numaccesses,numfaults,numwrites);
}

  public static int check_refresh(int numframes, PTE[] RAM, int countersize, int page2evict, int[] counter_array, List<Integer> cycles_array, int pagenumber, int refresh_parameter, int cycle_left){
    int cycle_num = 0;
    int refresh_num = 0;
    //check cycle number
    if(pagenumber == 0) {
      cycle_num = cycles_array.get(pagenumber);
    }else{
      cycle_num = cycles_array.get(pagenumber) + cycle_left + 1;
    }
    if(cycle_num >= numframes){
      refresh_num = (int)cycle_num/refresh_parameter; 
    }
    //get cycle_left
    cycle_left = cycle_num % refresh_parameter;

    if(countersize < numframes && countersize > 0){
      //RAM not full
      for(int k = 0; k < countersize; k++){
        //first refresh once 
        if(refresh_num != 0){
          counter_array[k] = counter_array[k] >>> 1;
          //if it is referenced, change the first bit of counter to 1
          if(RAM[k].is_referenced() == 1){
            counter_array[k]+=128;
            RAM[k].set_referenced(0);
          }
          if(refresh_num > 1){
            for(int m = 0; m<refresh_num-1; m++){
              counter_array[k] = counter_array[k] >>> 1;
            }
          }
        }
        // System.out.println("RAM: " + k + " " + counter_array[k]);
      }
    }else if(countersize >= numframes){
      for(int k = 0; k < numframes; k++){
        if(refresh_num != 0){
          counter_array[k] = counter_array[k] >>> 1;
          if(RAM[k].is_referenced() == 1){
            counter_array[k]+=128;
            RAM[k].set_referenced(0);
          }
          if(refresh_num>1){
            for(int m = 0; m<refresh_num-1; m++){
              counter_array[k] = counter_array[k] >>> 1;
            }
          } 
        }
        // System.out.println("RAM: " + k + " " + counter_array[k]);
      }
    }
    // System.out.println("cycle left: "+ cycle_left);
    return cycle_left;
  }

  public static void output(String alg, int numframes, int numaccesses, int numfaults, int numwrites)
  {
    System.out.println("Algorithm: "+alg);
    System.out.println("Number of frames: "+numframes);
    System.out.println("Total memory accesses: "+numaccesses);
    System.out.println("Total page faults: "+numfaults);
    System.out.println("Total writes to disk: "+numwrites);
  }
}