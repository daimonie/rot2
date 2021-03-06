#include <iostream>
#include <string>
#include <ctime>
#include "omp.h"
#include <stdio.h>
#include "data.h"
#include "simulation.h"   
#include "symmetryctwo.h"
#include "symmetryctwoh.h"
#include "symmetryctwov.h"
#include "symmetrydtwod.h"
#include "symmetrydtwoh.h"
#include "symmetrystwo.h"
#include "symmetrysfour.h"
#include "order.h"
#include "order_dummy.h"
#include "order_n.h"
#include "order_d2d.h"
//includes from previous implementation
#include <iostream>
#include <fstream> 
#include <string> 
#include <stdlib.h> 
#include <stdio.h>
#include <math.h>
#include <chrono> 
#include <gsl/gsl_math.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_cblas.h>
#include <iomanip> 
#include <ctime>
#include <string>
#include "omp.h"
#include <vector> 
#include <memory> 
//random generation libraries
#include "dSFMT-src-2.2.3/dSFMT.h"
#include <random> 
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/lagged_fibonacci.hpp>
#include <boost/random/uniform_01.hpp>
using namespace std;

int main(int argc, char* argv[])
{
    /***
    * 	Note: Always preface  with $$ for random information.
    * 	That way, the python functions will *ignore* the lines.
    ***/
    auto time_start = std::chrono::high_resolution_clock::now();
    //First, check if the arguments are proper.
    //Return 0 terminates program.  
    if(argc != 5)
    {
        fprintf(stderr, "$$ Three arguments are required, yet %d were given. \n", argc);
        return 0;
    } 

    string arg_symmetry	= string(argv[1]);
    string arg_size     = string(argv[2]);
    string arg_cores    = string(argv[3]);
    int arg_samples  = (int) atoi(argv[4]);

    double beta_number = 300.0; 
    int samples = 10;
    if(arg_size == "small")
    { 
        samples = 5000;
        fprintf(stderr, "$$ Will simulate small (6) lattice for point group %s, samples %d. \n", arg_symmetry.c_str(), samples); 
    }
    else if(arg_size == "large")
    {
        samples = 5000;
        fprintf(stderr, "$$ Will simulate large (10) lattice for point group %s, samples %d. \n", arg_symmetry.c_str(), samples);
    }
    else if(arg_size == "medium")
    {
        samples = 5000;
        fprintf(stderr, "$$ Will simulate large (8) lattice for point group %s, samples %d. \n", arg_symmetry.c_str(), samples);
    }
    else if(arg_size == "tiny")
    {
        samples = 25;
        beta_number = 10.0;
        fprintf(stderr, "$$ Will simulate test (4) lattice for point group %s, samples %d. \n", arg_symmetry.c_str(), samples);
    }
    else
    {
        fprintf(stderr, "$$ Unknown lattice size. \n");
        return 0;
    }

    if(arg_symmetry != "c2" && arg_symmetry != "c2h" && arg_symmetry != "c2v" && arg_symmetry != "d2d"
        && arg_symmetry != "d2h" && arg_symmetry != "s2" && arg_symmetry != "s4")
    {
        fprintf(stderr, "$$ Unknown point group. \n");
        return 0;
    } 
    //if the code is not terminated before this point, the arguments are well formed and we can just start calculating.

    //This should be fine, given that we copy from this object. Right?
    symmetry* gauge;

    if(arg_symmetry == "c2")
    {
        gauge = new symmetry_c2;
    }
    else if(arg_symmetry == "c2v")
    {
        gauge = new symmetry_c2v;
    }
    else if(arg_symmetry == "c2h")
    {
        gauge = new symmetry_c2h;
    }
    else if(arg_symmetry == "d2d")
    {
        gauge = new symmetry_d2d;
    }
    else if(arg_symmetry == "d2h")
    {
        gauge = new symmetry_d2h;
    }
    else if(arg_symmetry == "s2")
    {
        gauge = new symmetry_s2;
    }
    else if(arg_symmetry == "s4")
    {
        gauge = new symmetry_s4;
    }
    else
    {
        fprintf(stderr, "$$ Something went wrong. Terminating. \n");
        return 0;
    }
    fprintf(stderr, "$$ Setting gauge group to %s. \n", gauge->label().c_str());

    /***
    * We will simulate 0.1 < J < 1 at dJ = 0.01, and 1 < J < 2 at Dj=0.05.
    * We will thus require 99+20 values.	  
    ***/
    int imax = 48;

    vector<vector<data>> results;

    results.resize(imax);

    int lattice_size = 6;
    if(arg_size == "large")
    { 
        lattice_size = 10;
    }   
    else if(arg_size == "tiny")
    { 
        lattice_size = 4; 
        imax = 4; //local testing
    }    
    else if(arg_size == "medium")
    { 
        lattice_size = 8; 
    }    

    if( imax > omp_get_max_threads() )
    {
        fprintf(stderr, "Error: iMax > nproc.  \n");
        imax = omp_get_max_threads(); 
    } 
    //opens file, discards contents if exists
    //this is apparently C code, not C++, but it works and is simple
    FILE * backup_file_handler = fopen( ".simulation_backup", "w+");
    setbuf(backup_file_handler, NULL);

    if(arg_cores == "one")
    {
        imax = 1;
        samples = arg_samples;
    }
    else if(arg_cores == "two")
    {
        imax = 2;
        samples = arg_samples;
    }
    else if(arg_cores == "three")
    {
        imax = 2;
        samples = arg_samples;
    }
    else
    {
        imax = omp_get_max_threads();
    }

    fprintf(stderr, "$$ Final samples %d, cores %d, beta steps %.3f, lattice_size %d. \n", samples, imax, beta_number, lattice_size);
    
    omp_set_num_threads(omp_get_max_threads());
    #pragma omp parallel for
    for(int i = 0; i < imax; i++) 
    { 	
        double beta_max = 0.0;
        simulation sweep( lattice_size );
        sweep.build_gauge_bath( gauge );

        /***
        *  Explanation by Ke:
        * 
        *  tau is important there to make sure two samples are not autocorrelated.
        *  It estimate by a lattice with the size L = 12, for which the typical
        *  autocorrelation time is about 40 Monte Carlo steps. So tau = 100 makes
        *  two samples neighboring in time correlated by a factor \sim e^{-2} \sim 0.1.
        ***/
        sweep.tau = 100;
        sweep.dice_mode = 4;
        sweep.generate_rotation_matrices ();
        sweep.accuracy = 0.05;
        if(arg_cores == "two"  or arg_cores == "one" or arg_cores == "three")
        {
            sweep.j_one = -0.1;
            sweep.j_two = -0.1;
            sweep.j_three = -1.0;
            
            beta_max = 5.0;
        }
        else
        {
            sweep.j_one = 1 * (i+1) * 2.00 / imax;
            sweep.j_one *= -1; 
            sweep.j_two = sweep.j_one;
            sweep.j_three = -1.0;

            order * order_one = new order_n();
            order * order_two = new order_d2d();  

            sweep.set_order(0, order_one);
            sweep.set_order(1, order_two); 

            beta_max = 8.5/(2.00)*sweep.j_one + 10.0;  
        }

        sweep.sample_amount = samples; 

        //start at beta=0 (T=inf), then go to Bmax, then go down again. Hence, random.
        sweep.random_initialization ();
        sweep.mpc_initialisation ();

        for (int ii = 0; ii < sweep.length_three; ii++)
        {
            sweep.e_total += sweep.site_energy(ii);
        }
        sweep.e_total /= 2;
        sweep.e_ground = sweep.length_three*3*(sweep.j_one + sweep.j_two + sweep.j_three); 


        if(beta_max > 11.0 || beta_max < 0)
        {
            fprintf(stderr, "Warning: beta_max=%.3f out of bounds for J = %.3f.\n", beta_max, sweep.j_one);
        } 
        // cool it down (Tinf -> T finite)
        sweep.beta = 0.0;
        while( sweep.beta <= beta_max )
        { 	
            sweep.beta += beta_max / beta_number;
            sweep.thermalization (); 

            results[i].push_back(sweep.calculate ());   

            results[i][results[i].size()-1].shout(backup_file_handler);
        }    
    } 
    for(unsigned int i = 0; i < results.size(); i++)
    { 
        for(unsigned int jj = 0; jj < results[i].size(); jj++)
        {
            auto result = results[i][jj];
            result.report ();
        }
    }
    //report time, end program 
    auto time_end = std::chrono::high_resolution_clock::now();        
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>( time_end - time_start).count();
    fprintf(stderr, "$$ Elapsed time %ld microseconds. \n", microseconds); 
    fprintf(backup_file_handler, "$$ Elapsed time %ld microseconds. \n", microseconds); 
    //gauge was new'd, so it should be deleted
    delete gauge; 
    return 0;
}

