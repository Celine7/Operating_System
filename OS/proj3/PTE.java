//Haoyue Cui(hac113)
class PTE{
	private boolean exist;
	private int referenced;
	private int dirty;
	private int address;
	private int frame;
	private int counter;
	public PTE(){
		exist = false;
		referenced = -1;
		dirty = 0;
		address = -1;
		frame = -1;
	}
	public boolean is_exist(){
		return exist;
	} 
	public int is_referenced(){
		return referenced;
	} 
	public int is_dirty(){
		return dirty;
	} 
	public int get_address(){
		return address;
	} 
	public int get_frame(){
		return frame;
	} 
	public void set_exist(boolean exist){
		this.exist = exist;
	} 
	public void set_referenced(int referenced){
		this.referenced = referenced;
	} 
	public void set_dirty(int dirty){
		this.dirty = dirty;
	} 
	public void set_address(int address){
		this.address = address;
	} 
	public void set_frame(int frame){
		this.frame = frame;
	} 
}