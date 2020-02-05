#include <stdio.h>
#include <pthread.h>
#include <string>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <time.h>
#include <queue>
using namespace std;
//the struct that keeps all the parameters related to a customer
struct client{
	int sleepTime;
	int atm;
	int bill;
	int amount;
	int customerNo;
};

//global variable that can be reached by all threads
std::ofstream outputfile; // the output file
string out; //the name of the output file
pthread_mutex_t atm[10]; //the mutexs that are locked when customers are added to or removed from the queue of each ATM instance
pthread_mutex_t billtype[5]; //the mutexs that are locked when the total amout paid for each bill type is incremented
pthread_mutex_t writeToFile = PTHREAD_MUTEX_INITIALIZER; //the mutex that is locked when a thread is writing to the output file
int bills[5]; //the total amount that is paid for each bill type
string types[5]; //the string representation of the bill types
bool runatm = true; //the boolean variable that determines whether all customers have submitted their requests to the ATMs or not
struct client* myQueues[10][300]; //the waiting customer queues for each ATM instance
int index[10]; //the first unused spot in the customer queue for each ATM instance
bool isFinished[300];//the boolean variable that determines whether the request of the customer is realized

//adds a new customer to the queue
void addToQueue(int no, struct client* job){
    int j;
    //shifts all customers in the corresponding ATM queue once
    for (j=index[no-1]; j>0; j--){
        myQueues[no-1][j] = myQueues[no-1][j-1];
    }
    //adds the new customer to the corresponding ATM queue
    myQueues[no-1][0] = job;
    //increments the index of the corresponding ATM queue
    index[no-1]++;
}

//gets the next customer from the queue
struct client* getFromQueue(int no){
    //if the index of the corresponding ATM queue is 0, the queue is empty, therefore returns NULL
    if (index[no-1]==0){
        return NULL;
    }
    //the last used spot in the corresponding ATM queue is at index-1
    //this customer is the next customer in the queue
    struct client* next = myQueues[no-1][index[no-1]-1];
    myQueues[no-1][index[no-1]-1] = NULL;
    //decrements the index of the corresponding ATM queue
    index[no-1]--;
    //returns the next customer in the queue
    return next;
}

//ATM threads run here
void *atmInstance(void *param){
    //the ID of the ATM instance is extracted from the parameter of the function
    int *num = (int*)param;
    //the thread runs as long as customer threads are still submitting requests to the ATMs or the queue of the ATM is not empty
    while(runatm || index[(*num)-1]!=0){
        struct client* newJob;
        //the mutex for the ATM is locked and the ATM gets the next customer from the queue, then the mutex is unlocked
        pthread_mutex_lock(&atm[(*num)-1]);
        newJob = getFromQueue(*num);
        pthread_mutex_unlock(&atm[(*num)-1]);
        //the loop continues if there is no waiting customer in the queue
        if (newJob == NULL){
            continue;
        }
        //if there is a customer, the mutex for the corresponding bill type is locked
        pthread_mutex_lock(&billtype[(*newJob).bill]);
        //the total amount that is paid for that bill type is incremented by the amount that is paid by this customer
        bills[(*newJob).bill] = bills[(*newJob).bill] + ((*newJob).amount);
        //the mutex is unlocked again
        pthread_mutex_unlock(&billtype[(*newJob).bill]);
        //the mutex for writing to the output file is locked, the log is written to the file, the mutex is unlocked again
        pthread_mutex_lock(&writeToFile);
        outputfile.open(out, ofstream::app);
        outputfile << "Customer" << (*newJob).customerNo << "," << (*newJob).amount << "TL," << types[(*newJob).bill] << endl;
        outputfile.close();
        pthread_mutex_unlock(&writeToFile);
        //changes the isFinished variable to "true" since the payment is done
        isFinished[(*newJob).customerNo-1] = true;
    }
    pthread_exit(0);
}

//customer threads run here
void *customer(void *param){
    //the information about the customer is extracted from the struct client that is given to the function as a parameter
    struct client *thisClient = (struct client*)param;
    //the customer sleeps as long as his sleep time has not lapsed
    timespec sleep;
    int secs = (*thisClient).sleepTime/1000; //time in seconds
    sleep.tv_sec = (time_t)secs;
    sleep.tv_nsec = ((*thisClient).sleepTime%1000)*1000000; //time in nanoseconds
    nanosleep(&sleep, NULL);
    //the mutex for the corresponding ATM is locked and the customer is added to the queue of this ATM, then the mutex is unlocked
    pthread_mutex_lock(&atm[((*thisClient).atm)-1]);
    addToQueue(((*thisClient).atm),thisClient);
    pthread_mutex_unlock(&atm[((*thisClient).atm)-1]);
    //waits until the payment is done
    while(!isFinished[(*thisClient).customerNo-1]);
    pthread_exit(0);
}

int main(int argc, char *argv[]) {
    int number; //number of customers
    int i,j; //iterators
    int atmNum[10]; //array that holds the IDs of the ATM instances
    string temp; // string used to get the input
    string token; //string used to get the input
    string delimiter = ","; //string used to get the input
    pthread_t tid[300]; //thread IDs of the customers
    pthread_t tid2[10]; //thread IDs of the ATM instances
    struct client clients[300]; //array struct client that keeps the properties of the customer
    types[0] = "cableTV"; //string representations of the bill types
    types[1] = "electricity";
    types[2] = "gas";
    types[3] = "telecommunication";
    types[4] = "water";
    //initializes the queues to be empty
    for (i=0; i<10; i++){
        for(j=0; j<300; j++){
            myQueues[i][j] = NULL;
        }
    }
    //initializes the ATM mutexs, ATM IDs, and first unused indeces in the ATM's queues
    for (i=0; i<10; i++){
        atm[i] = PTHREAD_MUTEX_INITIALIZER;
        atmNum[i] = i+1;
        index[i] = 0;
    }
    //initializes the bill mutexs, and total amounts that are paid for each bill type
    for (i=0; i<5; i++){
        billtype[i] = PTHREAD_MUTEX_INITIALIZER;
        bills[i] = 0;
    }
    //creates ATM threads
    for (i=0; i<10; i++){
        pthread_create(&tid2[i], NULL, atmInstance, (void*)(&atmNum[i]));
    }
    //takes the input
    std::ifstream inputfile;
    inputfile.open(argv[1]);
    inputfile >> number;
    for (i=0; i<number; i++){
        isFinished[i] = false; //initializes the isFinished variable of each customer to "false" since the payment is not done yet
        inputfile >> temp;
        //parses each customer information with respect to the string ","
        token = temp.substr(0, temp.find(delimiter));
        clients[i].sleepTime = stoi(token);
        temp.erase(0, temp.find(delimiter)+1);
        token = temp.substr(0, temp.find(delimiter));
        clients[i].atm = stoi(token);
        temp.erase(0, temp.find(delimiter)+1);
        token = temp.substr(0, temp.find(delimiter));
        //turns the strings into their integer representations
        if (token == "cableTV"){
            clients[i].bill = 0;
        }
        else if (token == "electricity"){
            clients[i].bill = 1;
        }
        else if (token == "gas"){
            clients[i].bill = 2;
        }
        else if (token == "telecommunication"){
            clients[i].bill = 3;
        }
        else if (token == "water"){
            clients[i].bill = 4;
        }
        temp.erase(0, temp.find(delimiter)+1);
        clients[i].amount = stoi(temp);
        clients[i].customerNo = i+1;
    }
    inputfile.close();
    //creates the output file with the right name
    string in(argv[1]);
    in = in.substr(0, in.find("."));
    out = in + "_log.txt";
    outputfile.open(out);
    outputfile.close();
    //creates customer threads
    for (i=0; i<number; i++){
        pthread_create(&tid[i], NULL, customer, (void*)(&clients[i]));
    }
    //joins customer threads
    for (i=0; i<number; i++){
        pthread_join(tid[i], NULL);
    }
    //makes the boolean that keeps track of whether customer threads are still running false, no new request can be made now
    runatm = false;
    //joins the ATM threads
    for (i=0; i<10; i++){
        pthread_join(tid2[i], NULL);
    }
    //writes the final values of the total amounts that are paid to the output file
    outputfile.open(out, ofstream::app);
    outputfile << "All payments are completed." << endl;
    outputfile << "CableTV: " << bills[0] << "TL" << endl;
    outputfile << "Electricity: " << bills[1] << "TL" << endl;
    outputfile << "Gas: " << bills[2] << "TL" << endl;
    outputfile << "Telecommunication: " << bills[3]<< "TL" << endl;
    outputfile << "Water: " << bills[4]<< "TL" << endl;
    outputfile.close();
    return 0;
}
