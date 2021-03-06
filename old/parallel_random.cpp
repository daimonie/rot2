#include <iostream>
#include <string>
#include "omp.h"
using namespace std;

int s = 5;

inline void addition(int diff)
{
        s += diff;
}
int main(int argc, char **argv)
{
        printf("Small test for parallel stuff. \n");
        
        printf("s=%d \n", s);
        
        #pragma omp parallel for
        for(int j = 0; j < 4; j++)
        {
                for(int i = 0; i < 10; i++)
                {
                        s+= omp_get_thread_num()+1;  
                }
        }
        
        printf("s=%d \n", s);  
        
        s = 5;
        #pragma omp parallel for firstprivate(s)
        for(int j = 0; j < 4; j++)
        {
                for(int i = 0; i < 10; i++)
                {
                        s+= omp_get_thread_num()+1;  
                }
        }
        printf("s=%d \n", s);  
        
        
        s = 5;
        #pragma omp parallel for firstprivate(s)
        for(int j = 0; j < 4; j++)
        {
                for(int i = 0; i < 10; i++)
                {
                        addition(omp_get_thread_num()+1); 
                }
        }
        printf("s=%d \n", s);  
}