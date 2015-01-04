#include <stdlib.h>
#include <stdio.h>
#include "mpi.h"
#include <sys/time.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <omp.h>

#include <semaphore.h>
#include "common_types.h"

MPI_Datatype task_type;
struct task_list todo_list;
sem_t computers;
char end = 0, found = 0;
char finishing = 0;
char asking = 0;
char * pwd_found = NULL;
char nb_letters = 1;
char * pwd_given = NULL;
char * org = NULL;
int p, t, r, n;


inline void next_word(char* word){
  int i = 0;
  char remain = 1;
  while (remain == 1){
    word[i] = (word[i]+1)%nb_letters;
    if (word[i] != 0){
      remain = 0;
    } else {
      ++i;
      ++word[i];
    }
  }
  return;
}

inline char hash_verification(char* word){
  int i = 0;
  while ((word[i] !=  0) && (word[i] == pwd_given[i])){
    ++i;
  }
  if(pwd_given[i] == 0)
    return 1;
  return 0;
}

void thread_computation(){
  int i, k, word_size = 0;
  while(1){
    sem_wait(&computers);
    struct task * task_to_compute;
    if(!list_empty(&(todo_list.children))){
      // should critical section contain the list_empty_test ?
#pragma omp critical
      {
	task_to_compute = list_pop(&todo_list.children, struct task, list);
	--todo_list.num_children;
      }
      // task compute
      for (i = 0; i < task_to_compute->nb_test; ++i){
	if (hash_verification(task_to_compute->start_word) == 1){
	  pwd_found = malloc(sizeof(char)*(r+1));
	  for (k = 0; k <= r; ++k){
	    if (task_to_compute->start_word[k] != 0)
	      ++word_size;
	  }
	  for (k = 0; k < word_size; ++k){
	    pwd_found[k] = org[task_to_compute->start_word[word_size-k-1]];
	  }
	  pwd_found[k] = '\0';
	}
	next_word(task_to_compute->start_word);
      }
      //free(task_to_compute->start_word);
      //free(task_to_compute);
    } else { 
      if(end == 1)
	++finishing;
	return;
      sem_wait(&computers);
    }
  }
}

void thread_comm(MPI_Comm inter){
  int rank;
  MPI_Comm_rank(inter,&rank);
  printf("Slave %d starts comm\n", rank);
  while(1){
    int flag;
    int k;
    char buffer;
    struct task * task_to_deal_with;
    MPI_Status status_p;
    MPI_Iprobe(0, MPI_ANY_TAG, inter, &flag, &status_p);
    if (flag) {
      switch(status_p.MPI_TAG) {
	//Message asking for another tile. Here, the content of the message is an int corresponding to the original source of the message.
      case END :
#pragma omp critical
	{
	  while(!list_empty(&(todo_list.children))){
	    task_to_deal_with = list_pop(&todo_list.children, struct task, list);
	    //free(task_to_deal_with->start_word);
	    //free(task_to_deal_with);
	    --todo_list.num_children;    
	  }
	}
	end = 1;
	for(k = 0; k<t; k++)
	  sem_post(&computers);
	MPI_Recv(&buffer, 1, MPI_CHAR, 0, END, inter, MPI_STATUS_IGNORE);
	printf("Slave %d received END from master\n", rank);
	return;
	break;
      case FINISH :
	end = 1;
	for(k = 0; k<t; k++)
	  sem_post(&computers);
	MPI_Recv(&buffer, 1, MPI_CHAR, 0, FINISH, inter, MPI_STATUS_IGNORE);
	printf("Slave %d received FINISH from master\n", rank);
	break;
      case INTER :
	task_to_deal_with = malloc(sizeof(struct task));
	task_to_deal_with->start_word = malloc(sizeof(char)*(r+1));
	MPI_Recv(task_to_deal_with, 1, task_type, 0, INTER, inter, MPI_STATUS_IGNORE);
	#pragma omp critical
	{
	  list_add(&todo_list.children, &task_to_deal_with->list);
	  ++todo_list.num_children;
	  sem_post(&computers);
	}
	asking = 0;
	printf("Slave %d received INTER from master\n", rank);
	break;
      }
    } else {     // The process didn't receive anything, so you check if the process need other tasks
      if (pwd_found != NULL && found == 0) {
	struct task res;
	res.start_word = pwd_found;
	// Send an INTER message
	MPI_Send(&res, 1, task_type, 0, INTER, inter);
	// wake up all threads to end
	found = 1;
	//free(pwd_found);
      } else if (asking == 0 && todo_list.num_children < t){
	// we ask for a new interval
	MPI_Send(&buffer, 1, MPI_CHAR, 0, ASK, inter);
	asking = 1;
      } else if (finishing == t) {
	// we have nothing
	MPI_Send(&buffer, 1, MPI_CHAR, 0, NOTHING, inter);
	return;
      }
    } 
  }
}


// main function calling mpi functions
int main(int argc, char **argv){

  MPI_Comm inter;
  int myrank, size, provided;
  char c;
  char *a = NULL, *m = NULL;
  int i;

  while ((c = getopt (argc, argv, "p:t:a:r:m:")) != -1){
    switch (c) {
      case 'p':
      p = atoi(optarg);
      break;
      case 't':
      t = atoi(optarg);
      break;
      case 'a':
      a = optarg;
      break;
      case 'r':
      r = atoi(optarg);
      break;
      case 'm':
      m = optarg;
      break;
    }
  }
  
  //  printf("Read args: %d %s %d %s\n",t,a,r,m );

  list_head_init(&todo_list.children);
  todo_list.num_children = 0;

  MPI_Init_thread(&argc, &argv, MPI_THREAD_SERIALIZED, &provided);
  MPI_Comm_get_parent(&inter);
  
  int a_of_b[2] = {1,(1+r)};
  MPI_Aint a_of_d[2];
  MPI_Datatype a_of_t[2] = {MPI_INT,MPI_CHAR};
  MPI_Aint i1, i2, extent;
  struct task useless_task;
  MPI_Get_address(&useless_task, &i1);
  MPI_Get_address(&useless_task.nb_test, &i2); a_of_d[0] = i2-i1 ;
  MPI_Get_address(&useless_task.start_word, &i2); a_of_d[1] = i2-i1 ;
  MPI_Type_create_struct(2, a_of_b, a_of_d, a_of_t,&task_type);
  MPI_Type_commit(&task_type); // this type can represent an incoming task (besides start word) or the pwd found

  // determine alphabet nb_letters
  int alphabet_length = strlen(a);
  char start = 0;
  char occ[256];
  char translation[256];
  memset(translation, 0, 256);
  memset(occ, 0, 256);
  for (i = 0; i < alphabet_length; ++i){
    ++occ[a[i]];
    translation[a[i]] = ++start;
    
  }  

  for (i = 0; i < 256; ++i){
    if (occ[i] == 1)
      ++nb_letters;
  }

  org = malloc(sizeof(char)*(nb_letters));
  for (i = 0; i < 256; i++)
    if (translation[i] != 0)
      org[i] = translation[i];

  int pwd_len = strlen(m);
  pwd_given = malloc(sizeof(char)*(pwd_len+1));
  for (i = pwd_len - 1; i >= 0; --i){
    pwd_given[pwd_len  - 1 - i] = translation[m[i]];
  }
  pwd_given[pwd_len] = 0;

  omp_set_num_threads(t+1);
  #pragma omp parallel
  {
    n = omp_get_thread_num();
    if (n == 0)
      thread_comm(inter);
    else
      thread_computation();
  }

  sem_destroy(&computers);
  MPI_Finalize();
  return 0;
}
