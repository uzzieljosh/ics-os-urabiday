/* 
   ==========================================================================
   Console.c
   Author: Joseph Emmanuel Dayo
   Date updated:December 6, 2002
   Description: A kernel mode console that is used for debugging the kernel
   and testing new kernel features.
                
    DEX educational extensible operating system 1.0 Beta
    Copyright (C) 2004  Joseph Emmanuel DL Dayo

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. 
   ==========================================================================
*/

#include "console.h"


void runner(){
   int i=0;
   while(1){
      i++;
      i--;
   }
   //printf("Hello user thread!\n");
}

  
/*A console mode get string function terminates
upon receving \r */
void getstring(char *buf, DEX32_DDL_INFO *dev){
   unsigned int i=0;
   char c;
   do{
      c=getch();
      if (c=='\r' || c=='\n' || c==0xa) 
         break;

      if (c=='\b' || (unsigned char)c == 145){
         if(i>0){
            i--;
            if (Dex32GetX(dev)==0){
               Dex32SetX(dev,79);
               if (Dex32GetY(dev)>0) 
                  Dex32SetY(dev,Dex32GetY(dev)-1);
            }else{
               Dex32SetX(dev,Dex32GetX(dev)-1);
            }     
            Dex32PutChar(dev,Dex32GetX(dev),Dex32GetY(dev),' ',Dex32GetAttb(dev));
         };
      }else{
         if (i<256){  //maximum command line is only 255 characters
            Dex32PutChar(dev,Dex32GetX(dev),Dex32GetY(dev),buf[i]=c,Dex32GetAttb(dev));
            i++;
            Dex32SetX(dev,Dex32GetX(dev)+1);     
            if (Dex32GetX(dev)>79){
               Dex32SetX(dev,0);
               Dex32NextLn(dev);
            };
         };
      };

      Dex32PutChar(dev,Dex32GetX(dev),Dex32GetY(dev),' ',Dex32GetAttb(dev));
      update_cursor(Dex32GetY(dev),Dex32GetX(dev));
   }while (c!='\r');
    
   Dex32SetX(dev,0);
   Dex32NextLn(dev);
   buf[i]=0;
};

/*Show information about memory usage. This function is also useful
  for detecting memory leaks*/
void meminfo(){
   DWORD totalbytes = totalpages * 0x1000;
   DWORD freebytes = totalbytes - (totalpages - stackbase[0])* 0x1000;
   printf("=================Memory Information===============\n");
   printf("Pages available     : %10u pages\n",stackbase[0]);
   printf("Total Pages         : %10d pages\n",totalpages);
   printf("Total Memory        : %10u bytes (%10d KB)\n",totalbytes, totalbytes / 1024);
   printf("Free Memory         : %10u bytes (%10d KB)\n",freebytes, freebytes / 1024);
   printf("Used pages          : %10d pages (%10d KB)\n",totalpages-stackbase[0],
   (totalpages-stackbase[0])*0x1000);
};


int delfile(char *fname){
   int sectors;
   file_PCB *f=openfilex(fname,FILE_WRITE);
   return fdelete(f);
};


/*
 * Forks a new process
 */
int user_fork(){
   int curval = current_process->processid;

   int childready = 0, retval = 0;
   int hdl;
   int id;
   DWORD flags;

   #ifdef DEBUG_FORK
   printf("user_fork called\n");
   #endif
    
   //enable interrupts since we want the process dispatcher to take control
   storeflags(&flags);
   startints();
   
   //Calls pd_forkmodule() from kernel/process/pdispatch.c 
   hdl = pd_forkmodule(current_process->processid);

   //inform the CPU scheduler, hopefully to schedule process_dispatcher() 
   taskswitch();  

   //id = pd_ok(hdl); 
   id = pd_dispatched(hdl);
   while (!(id = pd_dispatched(hdl))){ //wait for the process to be dispatched before returning
      taskswitch(); //try to wakeup process_dispatcher() 
      ; 
   }

   if (curval != current_process->processid){ //this is the child
      //If this is the child process, the processid when this function
      //was called is not equal to the current processid.
      //pd_ok(hdl);
      retval = 0;
   };
      
   if (curval == current_process->processid){ // this is the parent
      pd_ok(hdl);          //free the createp_queue node used by the child
      retval = id;
   };
      
   restoreflags(flags);
   return retval;
};


/**
 * Function that reads an executable and creates a new process for it.
 */
int user_execp(char *fname, DWORD mode, char *params){
   DWORD id,size;
   char *buf;
   vfs_stat filestat;
   file_PCB *f = openfilex(fname, 0);              //Open the executable
  
   if (f!=0){                                      //The executable was successfully opened
      fstat(f,&filestat);                          //Get some statistics about the executable
      size = filestat.st_size;                     //Store the size of the executable
      buf = (char*)malloc(filestat.st_size+511);   //Allocate memory for the executable
      
      //tell the vfs to increase buffersize to speed up reads
      vfs_setbuffer(f, 0, filestat.st_size, FILE_IOFBF);

      if (fread(buf, size, 1, f) == size){         //A successful read of the executable
         char temp[255];
         #ifdef DEBUG_USER_PROCESS
         printf("execp(): adding module..\n");
         #endif

        
         //Calls addmodule() from kernel/process/pdispatch.c 
         int hdl= addmodule(fname, buf, userspace, mode, params, showpath(temp), getprocessid());
        
         #ifdef DEBUG_USER_PROCESS
         printf("execp(): done.\n");
         #endif

         taskswitch();     //inform the CPU scheduler, hopefully to schedule process_dispatcher()

         #ifdef DEBUG-USER_PROCESS
         printf("execp(): parent waiting for child to finish\n");
         #endif
    
         //loop until the new process has been dispatched    
         while (!(id = pd_ok(hdl))) 
            ;
         
         fg_setmykeyboard(id);

         //wait for the child process to finish
         dex32_waitpid(id,0);

         //dex32_wait();

         fg_setmykeyboard(getprocessid());
      };
      free(buf);
      fclose(f);
      return id;
   };
   return 0;
};

int exec(char *fname, DWORD mode, char *params){
   DWORD id;
   char *buf;
   file_PCB *f=openfilex(fname,0);

   if (f!=0){
      DWORD size;
      char temp[255];
      vfs_stat fileinfo;
      fstat(f,&fileinfo);
      size = fileinfo.st_size;
     
      buf=(char*)malloc(size+511);
     
      if (fread(buf,size,1,f) == size)
         id=dex32_loader(fname, buf, userspace, mode, params, showpath(temp), getprocessid());
      
      free(buf);
      fclose(f);
      return id;
   };
   return 0;
;};

int user_exec(char *fname,DWORD mode,char *params){
   DWORD id;
   char *buf;
   file_PCB *f = openfilex(fname, FILE_READ);

   if (f != 0){
      int hdl,size;
      char temp[255];
      vfs_stat fileinfo;
      fstat(f, &fileinfo);
      size = fileinfo.st_size;
      buf=(char*)malloc(size+511);
      fread(buf, size, 1, f);
      hdl = addmodule(fname, buf, userspace, mode, params, showpath(temp), getprocessid());
      while (!pd_ok(hdl)) 
         ;
     
      free(buf);
      fclose(f);
      return id;
  };
  return 0;
};


int loadDLL(char *name, char *p){
   file_PCB *handle;
   int fsize; vfs_stat filestat;
   int hdl,libid;
   char *buf;

   handle=openfilex(name,FILE_READ);
   if (!handle) 
      return -1;
   fstat(handle,&filestat);
   vfs_setbuffer(handle,0,filestat.st_size,FILE_IOFBF);
   //get filesize and allocate memory
   fsize= filestat.st_size;
   buf=(char*)malloc(fsize);
 
   //load the file from the disk 
   fread(buf, fsize, 1, handle);
 
   /*Tell the process dispatcher to map the file into memory and
      create data structures necessary for managing dynamic libraries*/
   hdl = addmodule(name, buf, lmodeproc, PE_USERDLL, p, 0, getprocessid());
 
   //wait until the library has been loaded before we continue since addmodule returns immediately
   while (!(libid = pd_ok(hdl)))
      ;
 
   //done!
   free(buf);

   //close the file
   fclose(handle);
   return libid;
};

void loadfile(char *s,int,int);

void loadlib(char *s){
   char *buf;
   DWORD size;
   loadDLL(s,0);
};


int console_showfile(char *s, int wait){
   char *buf;
   DWORD size;
   file_PCB *handle;
   vfs_stat fileinfo;
   int i;
   DEX32_DDL_INFO *myddl;
   handle=openfilex(s, FILE_READ);

   if (!handle) 
      return -1;
   fstat(handle,&fileinfo);
   size = fileinfo.st_size;
   buf=(char*)malloc(size);
   textbackground(BLUE);
   myddl = Dex32GetProcessDevice();
   printf("Name: %s  Size: %d bytes \n", s, size);
   textbackground(BLACK);
   fread(buf, size, 1, handle);
   for (i=0; i<size; i++){
      if (buf[i]!='\r') 
         printf("%c",buf[i]);
      if (myddl->lines%25==0){ 
         char c;
         printf("\nPress any key to continue, 'q' to quit\n");
         c=getch();
         if (c=='q') 
            break;
      };
   };
   fclose(handle);
   free(buf);
   return 1;
};


//creates a virtual console for a process
DWORD alloc_console(){
   dex32_commit(0xB8000, 1, current_process->pagedirloc, PG_WR);
};

void console(){
   console_main();
};


void prompt_parser(const char *promptstr, char *prompt){
   int i, i2 = 0, i3 = 0;
   char command[10], temp[255];
   strcpy(prompt,"");
   for (i=0; promptstr[i] && i<255; i++){
      if (promptstr[i] != '%'){ //add to the prompt
         prompt[i2]=promptstr[i];
         i2++;
         prompt[i2]=0;
      }else{
         if (promptstr[i+1] != 0){
            if (promptstr[i+1] == '%'){
               prompt[i2] = '%';
               i2++;
               prompt[i2] = 0;
               i+=2;
               continue;
            };
         };
         i3=0;
         for (i2=i+1; promptstr[i2]&&i2 < 255; i2++){
            if (promptstr[i2] == '%' || i3 >= 10) 
               break;
            command[i3]=promptstr[i2];
            i3++;
         };
         i=i2;
         command[i3]=0;
         if (strcmp(command,"cdir")==0){
            strcat(prompt, showpath(temp));
            i2=strlen(prompt);
         };
      };
   };
};
  

void kernel_file_io_demo(){
   char msg[10]="Hello!";  
   char buf[10];  
   file_PCB *fp;

   fp=openfilex("sample.txt",FILE_WRITE);
   fwrite(msg,sizeof(msg),1,fp);
   fclose(fp);
   
   fp=openfilex("sample.txt",FILE_READ);
   fread(buf,sizeof(msg),1,fp);
   printf("%s\n",buf);
   fclose(fp);

}



/*An auxillary function for qsort for comparing two elements, based on size*/
int console_ls_sortsize(vfs_node *n1, vfs_node *n2){
   if (n1->size > n2->size) 
      return -1;
   if (n2->size > n1->size) 
      return 1;
   return 0;
};

/*An auxillary function for qsort for comparing two elements, based on name*/
int console_ls_sortname(vfs_node *n1, vfs_node *n2){
   if ( (n1->attb & FILE_DIRECTORY) && !(n2->attb & FILE_DIRECTORY))
      return -1;

   if ( !(n1->attb & FILE_DIRECTORY) && (n2->attb & FILE_DIRECTORY))
      return 1;
        
   return strsort(n1->name,n2->name);
};

/* ==================================================================
   console_ls(int style):
   
   *list the contents of the current directory to the screen
    style = 1      : list format 
    style = others : wide format  
*/
/*lists the files in the current directory to the console screen*/
void console_ls(int style, int sortmethod){
   vfs_node *dptr=current_process->workdir;
   vfs_node *buffer;
   int totalbytes=0, freebytes=0;
   int totalfiles=0, i;
   char cdatestr[20], mdatestr[20], temp[20];
    
   //obtain total number of files
   totalfiles = vfs_listdir(dptr, 0, 0);
    
   buffer = (vfs_node*) malloc( totalfiles * sizeof(vfs_node));

   //Place the list of files obtained from the VFS into a buffer
   totalfiles = vfs_listdir(dptr, buffer, totalfiles * sizeof(vfs_node));     
    
   //Sort the list
   if (sortmethod == SORT_NAME)
      qsort(buffer, totalfiles, sizeof(vfs_node), console_ls_sortname);
   else if (sortmethod == SORT_SIZE)
      qsort(buffer, totalfiles, sizeof(vfs_node), console_ls_sortsize);
        
   textbackground(BLUE);
   textcolor(WHITE);
    
   if (style==1)
      printf("%-25s %10s %14s %14s\n","Filename", "Size(bytes)", "Attribute", "Date Modified");
        
   textbackground(BLACK);

   for (i=0; i < totalfiles; i++){
      char fname[255];   
      if (style == 0){ //wide view style
         if (buffer[i].attb&FILE_MOUNT)
            textcolor(LIGHTBLUE);
         else if (buffer[i].attb&FILE_DIRECTORY)
            textcolor(GREEN);
         else if (buffer[i].attb&FILE_OEXE)
            textcolor(YELLOW);
         else
            textcolor(WHITE);

         strcpy(fname,buffer[i].name);
         fname[24]=0;
         printf("%-25s ",fname);
         totalbytes+=buffer[i].size;
            
         if ( (i+1)%3==0 && (i+1 < totalfiles) ) 
            printf("\n");

      };

      if (style == 1){ //list view style
         if (buffer[i].attb&FILE_MOUNT)
            textcolor(LIGHTBLUE);
         else if (buffer[i].attb&FILE_DIRECTORY)
            textcolor(GREEN);
         else if (buffer[i].attb&FILE_OEXE)
            textcolor(YELLOW);
         else
            textcolor(WHITE);
                    
         strcpy(fname,buffer[i].name);
         fname[24]=0;
         printf("%-25s ",fname);
            
         textcolor(WHITE);
         printf("%10d %14s %14s\n",buffer[i].size,
         vfs_attbstr(&buffer[i],temp), datetostr(&buffer[i].date_modified,
                       mdatestr));
                       
         totalbytes+=buffer[i].size;


         //try to make it fit the screen
         if ((i+1) % 23==0){
            char c;
            printf("Press Q to quit or any other key to continue ...");
            c=getch();
            printf("\n");
            if (c=='q'||c=='Q') 
               break;
         };
      };       
   };
    
   textcolor(WHITE);
   printf("\nTotal Files: %d  Total Size: %d bytes\n", totalfiles, totalbytes);
   free(buffer);
    
};

/* ==================================================================
   console_execute(const char *str):
   * This command is used to execute a console string.

*/
int console_execute(const char *str){
   char temp[512];
   char *u;
   int command_length = 0;
   signed char mouse_x, mouse_y, last_mouse_x=0, last_mouse_y=0;
  
   //make a copy so that strtok wouldn't ruin str
   strcpy(temp,str);
   u=strtok(temp," ");
  
   if (u == 0) 
      return;
  
   command_length = strlen(u);    
    
   //check if a pathcut command was executed
   if (u[command_length - 1] == ':'){
      char temp[512];
      sprintf(temp,"cd %s",u);            
      console_execute(temp); 
   }else 
   if (strcmp(u,"fgman") == 0){  //--  Foreground manager
      fg_set_state(1);
   }else 
   if (strcmp(u,"mouse") == 0){  //--  Activate the mouse
      while (!kb_ready()){
         get_mouse_pos(&mouse_x,&mouse_y);
         printf("Mouse (x,y): %d %d\n",mouse_x, mouse_y);
         while ((last_mouse_x == mouse_x) && (last_mouse_y==mouse_y)){
            get_mouse_pos(&mouse_x,&mouse_y);
         }
         last_mouse_x=mouse_x;
         last_mouse_y=mouse_y; 
      }
   }else 
   if (strcmp(u,"shutdown") == 0){  //-- Shuts down the system.
      sendmessage(0,MES_SHUTDOWN,0);
   }else
   if (strcmp(u,"procinfo") == 0){  //-- Show process information. Args: <pid>
      int pid;             
      u=strtok(0," ");
      if (u!=0){
         pid = atoi(u);
         show_process_stat(pid);
      };
   }else
   if (strcmp(u,"meminfo") == 0){   //-- Show memory map information.
      mem_interpretmemory(memory_map,map_length);
   }else
   if (strcmp(u,"pause") == 0){     //-- Waits for a key press
      printf("press any key to continue or 'q' to quit..\n");
      if (getch() == 'q') 
         return -1;
   }else
   if (strcmp(u,"lspcut") == 0){    //-- Shows a list of path aliases. 
      vfs_showpathcuts();
   }else
   if (strcmp(u,"pcut") == 0){      //-- Creates a path alias. Args: <alias:> [path]
      char *u2,*u3;
      u2 = strtok(0," ");
      u3 = strtok(0," ");
      if (u2 != 0){
         if (vfs_addpathcut(u2,u3) == -1){
            printf("Invalid pathcut specified.\n");
         }else{
            printf("Pathcut added.\n"); 
         }                               
      }else{
         printf("Wrong number of parameters specified.\n");
      }
   }else
   if (strcmp(u,"rmdir") == 0){     //-- Removes a directory and all its subdirectories. Args: <dirname>
      char *u2 = strtok(0," ");
      if (u2 != 0){
         char c;                
         printf("*Warning!* This will delete all files and subdirectories!\n");
         printf("Do you wish to continue? (y/n):");
         c = getch();
         if (c == 'y'){
            printf("Please wait..\n");                        
            if (rmdir(u2) != -1)
               printf("Remove directory successful!\n");
            else
               printf("Error while removing directory.\n");
         }else{
            printf("Remove directory cancelled.\n");
         }
      }else{
         printf("Invalid parameter.\n"); 
      }
   }else
   if (strcmp(u,"rempcut") == 0){   //-- Removes a path alias. Args: <alias:>
      char *u2;
      u2 = strtok(0," ");
      if (u2 != 0){
         if (vfs_removepathcut(u2) == -1)
            printf("Invalid Pathcut or pathcut not found\n");
         else
            printf("Pathcut removed.\n");   
      }else{
         printf("Wrong number of parameters specified\n");
      }               
   }else
   if (strcmp(u,"newconsole") == 0){   //-- Creates a new console.  
      //create a new console         
      console_new();
      printf("New console thread created.\n");                   
   }else  
   if (strcmp(u,"ver") == 0) {         //-- Shows version information.
      printf("%s\n",dex32_versionstring);
      printf("%s\n",OS_VERSION);
   }else
   if (strcmp(u,"cpuid") == 0){        //-- Displays CPU information. 
      hardware_cpuinfo mycpu;
      hardware_getcpuinfo(&mycpu);
      hardware_printinfo(&mycpu);
   }else            
   if (strcmp(u,"exit") == 0){         //-- Exits a console session.
      fg_exit();
      exit(0);              
   }else  
   if (strcmp(u,"echo") == 0){         //-- Displays a string. Args: <string>  
      u=strtok(0,"\n");
      if (u!=0)              
         printf("%s\n",u);
   }else  
   if (strcmp(u,"use") == 0){          //-- Tells the extension manager to use the extension: Args: <extension>  
      u=strtok(0," ");
      if (extension_override(devmgr_getdevicebyname(u),0) == -1){
         printf("Unable to install extension %s.\n",u);                
      };            
   }else        
   if (strcmp(u,"off") == 0){          //-- Power off the machine.
      dex32apm_off();
   }else
   if (strcmp(u,"files") == 0){        //-- Shows list of currently open files.
      file_showopenfiles();
   }else
   if (strcmp(u,"find") == 0){         //-- Finds a file.
      u=strtok(0," ");
      if (u != 0)
         findfile(u);
   }else
   if (strcmp(u,"dkill") == 0){         //-- Dirty kill a user process/thread. No cleanup is done. Args: <pid>
      u=strtok(0," ");
      if (u!=0){
         ps_user_kill(atoi(u));
      }
   }else
   if (strcmp(u,"kill") == 0){         //-- Kills a thread/process. Performs cleanup.  Args: <thread name>
      //kernel_file_io_demo();
      u=strtok(0," ");
      if (u!=0){
         dex32_killkthread_name(u);
      }
   }else
   if (strcmp(u,"procs") == 0 || strcmp(u,"ps") == 0){  //-- List the running processes. "ps" can also be used.
      show_process();
   }else
   if (strcmp(u,"cls") == 0 || strcmp(u,"clear") == 0){          //-- Clears the screen. 
      clrscr();
      unsigned char stk[10240];
      //createkthread((void *)runner,"runner",10240);
      //createthread((void*)runner,stk,10240);
   }else
   if (strcmp(u,"help") == 0){         //-- Displays this help screen.
      console_execute("type /icsos/icsos.hlp");
   }else
   if (strcmp(u,"umount") == 0){       //-- Unmounts a mounted device. Args: <mount point>
      char *u =strtok(0," ");
      if (u!=0){
         if (vfs_unmount_device(u)==-1)
            printf("umount failed.\n");
         else
            printf("%s umounted.\n",u);
      }else{
         printf("Missing parameter.\n");
      }                    
   }else
   if (strcmp(u,"mount") == 0){        //-- Mounts a device. Args: fat/cdfs <partition/device> <mount point> 
      char *fsname,*devname,*location;
      fsname=strtok(0," ");
      devname=strtok(0," ");
      location=strtok(0," ");
               
      if (vfs_mount_device(fsname, devname, location) == -1)
         printf("mount not successful.\n");
      else
         printf("mount successful.\n");  
         //fat12_mount_root(root,floppy_deviceid);
   }else
   if (strcmp(u,"pwd") == 0){         //-- Shows the current working directory.
      char temp[255];
      printf("%s\n",showpath(temp));
   }else
   if (strcmp(u,"lsmod") == 0){        //-- Shows the list of loaded libraries and modules. 
      showlibinfo();
   }else
   if (strcmp(u,"mem") == 0){          //-- Shows memory information.
      meminfo();
   }else
   if (strcmp(u,"mkdir") == 0){        //-- Creates a directory. Args: <directory name> 
      u=strtok(0," ");
      if (u!=0){
         if (mkdir(u) == -1)
            printf("mkdir failed.\n");
      }
   }else       
   if (strcmp(u,"run") == 0){          //-- Executes a batch file or script. Args: <script>
      u=strtok(0," ");
      if (u!=0){
         if (script_load(u) == -1){
            printf("console: Error loading script file.\n");
         };            
      }
   }else    
   if (strcmp(u,"ls") == 0||strcmp(u,"dir") == 0){ //-- Shows directory listing. Args: [-l | -osize | -oname] 
      int style=0, ordering = 0;
      char v[20];
   
      u=strtok(0," ");
      if (u != 0){
         do {
            strcpy(v,u);
            if (strcmp(v,"-l") == 0) 
               style=1;
            if (strcmp(v,"-oname") == 0) 
               ordering  = 0;
            if (strcmp(v,"-osize") == 0) 
               ordering  = 1;
            u=strtok(0," ");
         } while (u!=0);
      };
      console_ls(style, ordering);
   }else
   if (strcmp(u,"del") == 0){             //-- Deletes a files or directory. Args: <filename/dirname>
      int res;
      u=strtok(0," ");
      if (u!=0){
         char *u3=strtok(0," ");
         if (u3==0){
            delfile(u);
            printf("File deleted.\n");
         }else{
            printf("Invalid parameter.\n");
         }
      }else{
         printf("Missing parameter.\n");
      }
   }else
   if (strcmp(u,"ren") == 0){            //-- Renames a file. Args: <oldname> <newname>
      char *u2,*u3;
      u2=strtok(0," ");
      u3=strtok(0," ");               
      if (u2!=0 && u3!=0){
         if (rename(u2, u3)) 
            printf("File renamed.\n");
         else
            printf("Error renaming file.\n");
      }else{
         printf("Missing parameter.\n"); 
      }   
   }else
   if (strcmp(u,"type") == 0 || strcmp(u,"cat") == 0 ){ //-- Displays the contents of a file. Args: <filename> [-p]
      u=strtok(0," ");
      if (u!=0){
         if (console_showfile(u,0)==-1)
            printf("error opening file.\n");
      }else{
         printf("missing parameter.\n");
      }
   }else
   if (strcmp(u,"copy") == 0 || strcmp(u,"cp") == 0){ //-- Copy source to destination: Args: <source> <destination>
      u=strtok(0," ");
      if (u!=0){
         char *u2 = strtok(0," ");
         if (u2!=0){
            if (fcopy(u,u2) == -1){
               printf("Copy failed. Error while copying.\n");
               printf("Destination directory might not be present.\n");
            };
         };  
      };
   }else              
   if (strcmp(u,"cd") == 0){     //-- Changes working directory. Args: <directory>
      u=strtok(0," ");
      if (u!=0){
         if (!changedirectory(u))
            printf("cd: Cannot find directory\n");
      }else{
         changedirectory(0); //go to working directory
      } 
   }else
   if (strcmp(u,"loadmod") == 0){   //-- Loads a shared library (.dll or .so). Args: <module filename> 
      u=strtok(0," ");
      if (u!=0){
         if (loadDLL(u,str) == -1)
            printf("Unable to load %s.\n",u);
         else
            printf("Load module Successful.\n");  
      }else{
            printf("missing parameter.\n");
      }
   }else
   if (strcmp(u,"lsdev") == 0){  //-- Lists all modules currently installed and available. 
      devmgr_showdevices();
   }else
   if (strcmp(u,"lsext") == 0){  //-- Lists all extensions.
      extension_list();
   }else
   if (strcmp(u,"libinfo") == 0){ //-- Shows library information.
      u=strtok(0," ");
      module_listfxn(u);
   }else
   if (strcmp(u,"time") == 0){   //-- Displays date and time.
      printf("%d/%d/%d %d:%d.%d (%d total seconds since 1970)\n",time_systime.day,
               time_systime.month, time_systime.year,
               time_systime.hour, time_systime.min,
               time_systime.sec,time());
   }else
   if (strcmp(u,"set") == 0){    //-- Sets an environment variable. Args: <key>=<value>
      u=strtok(0," ");
      if (u==0){
         env_showenv();
      }else{
         char *name  = strtok(u,"=");
         char *value = strtok(0,"\n");
         env_setenv(name, value, 1);
      }; 
   }else         
   if (strcmp(u,"unload") == 0){ //-- Unloads a library. Args: <library name>
      u=strtok(0," ");
      if (u!=0){
         if (module_unload_library(u) == -1)
            printf("Error unloading library");
   	};
   }else
   if (strcmp(u,"demo_graphics") == 0){   //-- Runs the graphics demonstration.
      demo_graphics();
   }else
   if (strcmp(u,"cc") == 0){   //-- Builds a C program (invokes tcc.exe). Args: <name.exe> <name.c>
      char src[30],exe[30],cmdline[256],path[256];
      char sdk_home[128]="";
      env_getenv("SDK_HOME",sdk_home);
      env_getenv("PATH",path);
      if ( (strcmp(sdk_home,"")==0) || strcmp(path,"")==0 ){
         printf("Please set the SDK_HOME and PATH environment variables first.\n");
      }else{
         u=strtok(0," ");
         if (u!=0){
            strcpy(exe,u);
            u=strtok(0," ");
            if (u!=0){
               strcpy(src,u);
               sprintf(cmdline,"%s/tcc.exe -o%s %s -B%s %s/tccsdk.c %s/crt1.c",
                        path,exe,src,sdk_home,sdk_home,sdk_home);
               user_execp("/icsos/apps/tcc.exe",0,cmdline);
            }else{
               printf("Usage: cc <name.exe> <name.c>\n");
            }
         }else{
               printf("Usage: cc <name.exe> <name.c>\n");
         }
      }
   }else
   if (strcmp(u,"add") == 0){ //-- Adds two integers. Args: <num1> <num2>
	int a, b;
	u = strtok(0," ");
	a = atoi(u);
	u = strtok(0," ");
	b = atoi(u);
	printf("%d + %d = %d\n",a,b,a+b);
   }else
   if (u[0] == '$'){                      //-- Sends message to a device.
      int i, devid;
      char devicename[255],*cmd;

      for (i=0;i<20 && u[i+1];i++){
         devicename[i] = u[i+1];
      };
      devicename[i] = 0;
      printf("Sending command to %s\n",devicename);
      devid = devmgr_finddevice(devicename);
               
      if (devid != -1){
         if (devmgr_sendmessage(devid,DEVMGR_MESSAGESTR,str)==-1)
            printf("console: send_message() failed or not supported.\n");
      }else{
         printf("console: cannot find device.\n");
      }   

   }else{         //ok it is not a command, maybe it's an executable?
      if (u!=0){
         char path[256]="", tmp[256];
         env_getenv("PATH",path);     
         if (strcmp(path,"")==0){
            strcpy(path,"/icsos/apps");
            sprintf(tmp,"%s/%s",path,u);
            if (!user_execp(tmp, 0, str)){
               printf("Command or executable not found.\n");
            }
         }else{
            sprintf(tmp,"%s/%s",path,u);
            if (!user_execp(tmp, 0, str)){
               printf("Command or executable not found.\n");
            }
         }
      }
   }
   //normal termination
   return 1;
};

int console_new(){
   //create a new console         
   char consolename[255];
   sprintf(consolename,"console(%d)", console_first);    
   return createkthread((void*)console, consolename, 200000);
};

void console_main(){
   DEX32_DDL_INFO *myddl=0;
   fg_processinfo *myfg;
   char s[256]="";
   char temp[256]="";
   char last[256]="";
   char console_fmt[256]="%cdir% %% ";
   char console_prompt[256]="cmd >";
    
   DWORD ptr;
    
   myddl =Dex32CreateDDL();    

    
   Dex32SetProcessDDL(myddl, getprocessid());
   myfg = fg_register(myddl, getprocessid());
   fg_setforeground( myfg->id );

   clrscr();
   strcpy(last,"");
    
   if (console_first == 0) 
      script_load("/icsos/autoexec.bat");
    
   console_first++;  
   do{
      textcolor(WHITE);
      textbackground(BLACK);
      prompt_parser(console_fmt,console_prompt);
    
      textcolor(LIGHTBLUE);
      printf("%s",console_prompt);
      textcolor(WHITE);
    
      if (strcmp(s,"@@")!=0 && strcmp(s,"!!")!=0)
         strcpy(last,s);
    
      getstring(s, myddl);
   
      if (strcmp(s,"!")==0){
         sendtokeyb(last,&_q);
      }
      else if (strcmp(s,"!!")==0){
         sendtokeyb(last,&_q);
         sendtokeyb("\r",&_q);
      }
      else   
         console_execute(s);
   } while (1);
};

