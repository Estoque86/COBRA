/*
 * post.cc
 *
 *  Created on: 14/dic/2012
 *      Author: michele
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <stdint.h>
#include <cstdlib>
#include <math.h>
#include <sstream>
#include <fstream>
#include <limits>
#include <vector>
#include <boost/tokenizer.hpp>


int
main (int argc, char *argv[])
{
	//long double simDuration = 60000000000000;
	//long double simDuration = 115000000000000;
	long double simDuration = 11500000000000;
	//long double timeSlot = 20000000000;
	//long double timeSlot = 50000000000;
	long double timeSlot = 10000000000;
	//long double timeSlot = 2000000000;
	long double numSlot = simDuration / timeSlot;  // 600
	//long double slotMax = 2000;
	long double slotMax = 900;
	//long double slotMax = 57000;
	
	long double numero_sim = 10;
	int num_par_mis = 9;

	// **** Vettore contenente i parametri medi misurati;
	std::vector<std::vector<std::vector<long double> > >  parametriMisurati; // = new std::vector<std::vector<long double> > (16,numSlot,0);

	parametriMisurati.resize(num_par_mis);
	for (uint32_t i = 0; i < num_par_mis; ++i)
	{
		parametriMisurati[i].resize(numero_sim);
		for (uint32_t j = 0; j < numero_sim; ++j)
		{
			parametriMisurati[i][j].resize(numSlot, 0);
		}
	}

	long double ValoriInseriti = 0;     // Servirà per calcolare la media;
	long double ValoriInseritiFILE = 0;
	long double ValoriInseritiCACHE = 0;
	long double ValoriInseritiCLIENT = 0;
	long double ValoriInseritiCLIENT_NonRichiesti = 0;
	long double ValoriInseritiCLIENT_Dropped = 0;
	long double ValoriInseritiCORE = 0;
	long double ValoriInseritiCORE_NonRichiesti = 0;
	long double ValoriInseritiCORE_Dropped = 0;
	long double ValoriInseritiREPO = 0;
	long double ValoriInseritiLOCAL = 0;
	long double fileAperti = 0;
	long double fileApertiCLIENT = 0;
	long double fileApertiCORE = 0;
	long double fileApertiREPO = 0;

	bool repo = false;


	//long double interestInviatiCLIENT = 0;
	//long double dataRicevutiCLIENT = 0;

	bool primo = true;
	long double checkGener = 0;
	long double checkInterf = 0;


	std::vector<long double>* ValoriInseriti_sl = new std::vector<long double> (numSlot, 0);     // Servirà per calcolare la media;
	std::vector<long double>* ValoriInseritiCACHE_sl = new std::vector<long double> (numSlot, 0);
	std::vector<long double>* ValoriInseritiCLIENT_sl = new std::vector<long double> (numSlot, 0);
	std::vector<long double>* ValoriInseritiCLIENT_NonRichiesti_sl = new std::vector<long double> (numSlot, 0);
	std::vector<long double>* ValoriInseritiCLIENT_Dropped_sl = new std::vector<long double> (numSlot, 0);
	std::vector<long double>* ValoriInseritiCORE_sl = new std::vector<long double> (numSlot, 0);
	std::vector<long double>* ValoriInseritiCORE_NonRichiesti_sl = new std::vector<long double> (numSlot, 0);
	std::vector<long double>* ValoriInseritiCORE_Dropped_sl = new std::vector<long double> (numSlot, 0);
	std::vector<long double>* ValoriInseritiREPO_sl = new std::vector<long double> (numSlot, 0);
	std::vector<long double>* ValoriInseritiLOCAL_sl = new std::vector<long double> (numSlot, 0);
	//std::vector<long double>* fileAperti_sl = new std::vector<long double> (numSlot, 0);
	//std::vector<long double>* fileApertiCLIENT_sl = new std::vector<long double> (numSlot, 0);
	//std::vector<long double>* fileApertiCORE_sl = new std::vector<long double> (numSlot, 0);


	std::vector<long double>* interestInviatiCLIENT_sl = new std::vector<long double> (numSlot, 0);
	std::vector<long double>* interestInviatiCORE_sl = new std::vector<long double> (numSlot, 0);
	std::vector<long double>* interestRicevutiCLIENT_sl = new std::vector<long double> (numSlot, 0);
	std::vector<long double>* interestRicevutiCORE_sl = new std::vector<long double> (numSlot, 0);
	std::vector<long double>* interestRicevutiREPO_sl = new std::vector<long double> (numSlot, 0);
//	std::vector<long double>* interestScartatiCLIENT_sl = new std::vector<long double> (numSlot, 0);
//	std::vector<long double>* interestScartatiCORE_sl = new std::vector<long double> (numSlot, 0);
//	std::vector<long double>* interestScartatiREPO_sl = new std::vector<long double> (numSlot, 0);
	std::vector<long double>* dataRicevutiCLIENT_sl = new std::vector<long double> (numSlot, 0);
	std::vector<long double>* dataRicevutiCORE_sl = new std::vector<long double> (numSlot, 0);
	std::vector<long double>* dataInviatiCLIENT_sl = new std::vector<long double> (numSlot, 0);
	std::vector<long double>* dataInviatiCORE_sl = new std::vector<long double> (numSlot, 0);
	std::vector<long double>* dataInviatiREPO_sl = new std::vector<long double> (numSlot, 0);
//	std::vector<long double>* dataScartatiCLIENT_sl = new std::vector<long double> (numSlot, 0);
//	std::vector<long double>* dataScartatiCORE_sl = new std::vector<long double> (numSlot, 0);
//	std::vector<long double>* dataScartatiNonRichiestiCLIENT_sl = new std::vector<long double> (numSlot, 0);
//	std::vector<long double>* dataScartatiNonRichiestiCORE_sl = new std::vector<long double> (numSlot, 0);
//	std::vector<long double>* dataScartatiDroppedCLIENT_sl = new std::vector<long double> (numSlot, 0);
//	std::vector<long double>* dataScartatiDroppedCORE_sl = new std::vector<long double> (numSlot, 0);
	
	std::vector<long double>* downloadTimeFile_sl = new std::vector<long double> (numSlot, 0);
	std::vector<long double>* downloadTimeFileAllLocal_sl = new std::vector<long double> (numSlot, 0);
	std::vector<long double>* downloadTimeFirst_sl = new std::vector<long double> (numSlot, 0);
	std::vector<long double>* downloadTimeFirstAllLocal_sl = new std::vector<long double> (numSlot, 0);
	std::vector<long double>* interestEliminatiFile_sl = new std::vector<long double> (numSlot, 0);
	
	// *** TRACING DEGLI INTEREST INVIATI A LIVELLO APPLICATIVO (escluse rtx) e dei DATA RICEVUTI a livello applicativo (compresi quelli provenienti dalla cache locale del consumer)
	// *** I classici INTEREST INVIATI e DATA RICEVUTI contengono solo gli interest inoltrati al di fuori del nodo (non soddisf localmente) comprese le rtx, e i DATA ricevuti dall'esterno, rispettivamente.
	std::vector<long double>* dataRicevutiAPP_sl = new std::vector<long double> (numSlot, 0);
	std::vector<long double>* interestInviatiAPP_sl = new std::vector<long double> (numSlot, 0);
	
	std::vector<long double>* distanceFirst_sl = new std::vector<long double> (numSlot, 0);


	// Ogni parametro misurato viene posto in una matrice avente per righe il numero di ripetizioni (3) e per colonne il numero degli slot temporali (numSlot);

	std::vector<std::vector<long double> > dataInviatiCACHE; //  = new std::vector<long double> (numSlot,0);    		// Si tiene traccia per tutti i nodi;
	dataInviatiCACHE.resize(numero_sim);//resize(3);
	for (uint32_t i = 0; i < numero_sim; ++i)
	{
		dataInviatiCACHE[i].resize(numSlot, 0);
	}



	// **** Stringhe per accedere ai file da analizzare e matrici dei parametri misurati******

//	std::string DOWNLOAD = "DOWNLOAD/DownloadTime_Nodo_";
//	std::vector<std::vector<long double> > downloadVect; //= new std::vector<long double> (3,0);
//	downloadVect.resize(3);
//	for (uint32_t i = 0; i < 3; ++i)
//	{
//		downloadVect[i].resize(numSlot, 0);
//	}
	
    std::string DOWNLOAD_FILE = "DOWNLOAD/FILE/DownloadTimeFile_Nodo_";
	std::vector<std::vector<long double> > downloadFileVect; //= new std::vector<long double> (3,0);
	downloadFileVect.resize(numero_sim);
	for (uint32_t i = 0; i < numero_sim; ++i)
	{
		downloadFileVect[i].resize(numSlot, 0);
	}
	
    std::vector<std::vector<long double> > downloadFileAllLocalVect; //= new std::vector<long double> (3,0);
	downloadFileAllLocalVect.resize(numero_sim);
	for (uint32_t i = 0; i < numero_sim; ++i)
	{
		downloadFileAllLocalVect[i].resize(numSlot, 0);
	}

	
    std::string DOWNLOAD_FIRST = "DOWNLOAD/FIRST/DownloadTime_Nodo_";
	std::vector<std::vector<long double> > downloadFirstVect; //= new std::vector<long double> (3,0);
	downloadFirstVect.resize(numero_sim);
	for (uint32_t i = 0; i < numero_sim; ++i)
	{
		downloadFirstVect[i].resize(numSlot, 0);
	}
	
	std::vector<std::vector<long double> > downloadFirstAllLocalVect; //= new std::vector<long double> (3,0);
	downloadFirstAllLocalVect.resize(numero_sim);
	for (uint32_t i = 0; i < numero_sim; ++i)
	{
		downloadFirstAllLocalVect[i].resize(numSlot, 0);
	}




	std::string DATA_INVIATI = "DATA/Data_INVIATI/DataINVIATI_Nodo_";
	std::vector<std::vector<long double> > dataInviatiVect; //= new std::vector<long double> (3,0);
	dataInviatiVect.resize(numero_sim);
	for (uint32_t i = 0; i < numero_sim; ++i)
	{
		dataInviatiVect[i].resize(numSlot, 0);
	}
	std::vector<std::vector<long double> > dataInviatiClientVect; //= new std::vector<long double> (3,0);
	dataInviatiClientVect.resize(numero_sim);
	for (uint32_t i = 0; i < numero_sim; ++i)
	{
		dataInviatiClientVect[i].resize(numSlot, 0);
	}
	std::vector<std::vector<long double> > dataInviatiCoreVect; //= new std::vector<long double> (3,0);
	dataInviatiCoreVect.resize(numero_sim);
	for (uint32_t i = 0; i < numero_sim; ++i)
	{
		dataInviatiCoreVect[i].resize(numSlot, 0);
	}
	std::vector<std::vector<long double> > dataInviatiRepoVect; //= new std::vector<long double> (3,0);
	dataInviatiRepoVect.resize(numero_sim);
	for (uint32_t i = 0; i < numero_sim; ++i)
	{
		dataInviatiRepoVect[i].resize(numSlot, 0);
	}



	std::string DATA_RICEVUTI = "DATA/Data_RICEVUTI/DataRICEVUTI_Nodo_";
	std::vector<std::vector<long double> > dataRicevutiVect; //= new std::vector<long double> (3,0);
	dataRicevutiVect.resize(numero_sim);
	for (uint32_t i = 0; i < numero_sim; ++i)
	{
		dataRicevutiVect[i].resize(numSlot, 0);
	}
	std::vector<std::vector<long double> > dataRicevutiClientVect; //= new std::vector<long double> (3,0);
	dataRicevutiClientVect.resize(numero_sim);
	for (uint32_t i = 0; i < numero_sim; ++i)
	{
		dataRicevutiClientVect[i].resize(numSlot, 0);
	}
	std::vector<std::vector<long double> > dataRicevutiCoreVect; //= new std::vector<long double> (3,0);
	dataRicevutiCoreVect.resize(numero_sim);
	for (uint32_t i = 0; i < 3; ++i)
	{
		dataRicevutiCoreVect[i].resize(numSlot, 0);
	}
	
	std::string DATA_RICEVUTI_APP = "DATA/Data_RICEVUTI/APP/Data_RICEVUTI_APP_Nodo_";
	std::vector<std::vector<long double> > dataRicevutiAppVect; //= new std::vector<long double> (3,0);
	dataRicevutiAppVect.resize(numero_sim);
	for (uint32_t i = 0; i < numero_sim; ++i)
	{
		dataRicevutiAppVect[i].resize(numSlot, 0);
	}


	//std::string DATA_SCARTATI = "DATA/Data_SCARTATI/DataSCARTATI_Nodo_";
	//std::vector<std::vector<long double> > dataScartatiVect; //= new std::vector<long double> (3,0);
	//dataScartatiVect.resize(3);
	//for (uint32_t i = 0; i < 3; ++i)
	//{
		//dataScartatiVect[i].resize(numSlot, 0);
	//}

	//std::vector<std::vector<long double> > dataScartatiClientVect; //= new std::vector<long double> (3,0);
	//dataScartatiClientVect.resize(3);
	//for (uint32_t i = 0; i < 3; ++i)
	//{
		//dataScartatiClientVect[i].resize(numSlot, 0);
	//}

	//std::vector<std::vector<long double> > dataScartatiCoreVect; //= new std::vector<long double> (3,0);
	//dataScartatiCoreVect.resize(3);
	//for (uint32_t i = 0; i < 3; ++i)
	//{
		//dataScartatiCoreVect[i].resize(numSlot, 0);
	//}
	
		//std::vector<std::vector<long double> > dataScartatiNonRichiestiClientVect; //= new std::vector<long double> (3,0);
	//dataScartatiNonRichiestiClientVect.resize(3);
	//for (uint32_t i = 0; i < 3; ++i)
	//{
		//dataScartatiNonRichiestiClientVect[i].resize(numSlot, 0);
	//}

	//std::vector<std::vector<long double> > dataScartatiDroppedClientVect; //= new std::vector<long double> (3,0);
	//dataScartatiDroppedClientVect.resize(3);
	//for (uint32_t i = 0; i < 3; ++i)
	//{
		//dataScartatiDroppedClientVect[i].resize(numSlot, 0);
	//}

    //std::vector<std::vector<long double> > dataScartatiNonRichiestiCoreVect; //= new std::vector<long double> (3,0);
	//dataScartatiNonRichiestiCoreVect.resize(3);
	//for (uint32_t i = 0; i < 3; ++i)
	//{
		//dataScartatiNonRichiestiCoreVect[i].resize(numSlot, 0);
	//}

	//std::vector<std::vector<long double> > dataScartatiDroppedCoreVect; //= new std::vector<long double> (3,0);
	//dataScartatiDroppedCoreVect.resize(3);
	//for (uint32_t i = 0; i < 3; ++i)
	//{
		//dataScartatiDroppedCoreVect[i].resize(numSlot, 0);
	//}


	std::string INTEREST_INVIATI = "INTEREST/Interest_INVIATI/InterestINVIATI_Nodo_";
	std::vector<std::vector<long double> > interestInviatiVect; //= new std::vector<long double> (3,0);
	interestInviatiVect.resize(numero_sim);
	for (uint32_t i = 0; i < numero_sim; ++i)
	{
		interestInviatiVect[i].resize(numSlot, 0);
	}
	std::vector<std::vector<long double> > interestInviatiClientVect; //= new std::vector<long double> (3,0);
	interestInviatiClientVect.resize(numero_sim);
	for (uint32_t i = 0; i < numero_sim; ++i)
	{
		interestInviatiClientVect[i].resize(numSlot, 0);
	}
	std::vector<std::vector<long double> > interestInviatiCoreVect; //= new std::vector<long double> (3,0);
	interestInviatiCoreVect.resize(numero_sim);
	for (uint32_t i = 0; i < numero_sim; ++i)
	{
		interestInviatiCoreVect[i].resize(numSlot, 0);
	}
	
	std::string INTEREST_INVIATI_APP = "INTEREST/Interest_INVIATI/APP/InterestINVIATI_APP_Nodo_";
	std::vector<std::vector<long double> > interestInviatiAppVect; //= new std::vector<long double> (3,0);
	interestInviatiAppVect.resize(numero_sim);
	for (uint32_t i = 0; i < numero_sim; ++i)
	{
		interestInviatiAppVect[i].resize(numSlot, 0);
	}



	std::string INTEREST_RICEVUTI = "INTEREST/Interest_RICEVUTI/InterestRICEVUTI_Nodo_";
	std::vector<std::vector<long double> > interestRicevutiVect; //= new std::vector<long double> (3,0);
	interestRicevutiVect.resize(numero_sim);
	for (uint32_t i = 0; i < numero_sim; ++i)
	{
		interestRicevutiVect[i].resize(numSlot, 0);
	}
	std::vector<std::vector<long double> > interestRicevutiClientVect; //= new std::vector<long double> (3,0);
	interestRicevutiClientVect.resize(numero_sim);
	for (uint32_t i = 0; i < numero_sim; ++i)
	{
		interestRicevutiClientVect[i].resize(numSlot, 0);
	}
	std::vector<std::vector<long double> > interestRicevutiCoreVect; //= new std::vector<long double> (3,0);
	interestRicevutiCoreVect.resize(numero_sim);
	for (uint32_t i = 0; i < numero_sim; ++i)
	{
		interestRicevutiCoreVect[i].resize(numSlot, 0);
	}
	std::vector<std::vector<long double> > interestRicevutiRepoVect; //= new std::vector<long double> (3,0);
	interestRicevutiRepoVect.resize(numero_sim);
	for (uint32_t i = 0; i < numero_sim; ++i)
	{
		interestRicevutiRepoVect[i].resize(numSlot, 0);
	}



	std::string INTEREST_AGGREGATI = "INTEREST/Interest_AGGREGATI/InterestAGGREGATI_Nodo_";
	std::vector<std::vector<long double> > interestAggregatiVect; //= new std::vector<long double> (3,0);
	interestAggregatiVect.resize(numero_sim);
	for (uint32_t i = 0; i < numero_sim; ++i)
	{
		interestAggregatiVect[i].resize(numSlot, 0);
	}

	std::string INTEREST_ELIMINATI = "INTEREST/Interest_ELIMINATI/InterestELIMINATI_Nodo_";
	std::vector<std::vector<long double> > interestEliminatiVect; //= new std::vector<long double> (3,0);
	interestEliminatiVect.resize(numero_sim);
	for (uint32_t i = 0; i < numero_sim; ++i)
	{
		interestEliminatiVect[i].resize(numSlot, 0);
	}
	
    std::string INTEREST_ELIMINATI_FILE = "INTEREST/Interest_ELIMINATI/FILE/InterestELIMINATI_FILE_Nodo_";
	std::vector<std::vector<long double> > interestEliminatiFileVect; //= new std::vector<long double> (3,0);
	interestEliminatiFileVect.resize(numero_sim);
	for (uint32_t i = 0; i < numero_sim; ++i)
	{
		interestEliminatiFileVect[i].resize(numSlot, 0);
	}


	std::string INTEREST_RITRASMESSI = "INTEREST/Interest_RITRASMESSI/InterestRITRASMESSI_Nodo_";
	std::vector<std::vector<long double> > interestRitrasmessiVect; //= new std::vector<long double> (3,0);
	interestRitrasmessiVect.resize(numero_sim);
	for (uint32_t i = 0; i < numero_sim; ++i)
	{
		interestRitrasmessiVect[i].resize(numSlot, 0);
	}

	//std::string INTEREST_SCARTATI = "INTEREST/Interest_SCARTATI/InterestSCARTATI_Nodo_";
	//std::vector<std::vector<long double> > interestScartatiVect; //= new std::vector<long double> (3,0);
	//interestScartatiVect.resize(3);
	//for (uint32_t i = 0; i < 3; ++i)
	//{
		//interestScartatiVect[i].resize(numSlot, 0);
	//}
	//std::vector<std::vector<long double> > interestScartatiClientVect; //= new std::vector<long double> (3,0);
	//interestScartatiClientVect.resize(3);
	//for (uint32_t i = 0; i < 3; ++i)
	//{
		//interestScartatiClientVect[i].resize(numSlot, 0);
	//}
	//std::vector<std::vector<long double> > interestScartatiCoreVect; //= new std::vector<long double> (3,0);
	//interestScartatiCoreVect.resize(3);
	//for (uint32_t i = 0; i < 3; ++i)
	//{
		//interestScartatiCoreVect[i].resize(numSlot, 0);
	//}
	//std::vector<std::vector<long double> > interestScartatiRepoVect; //= new std::vector<long double> (3,0);
	//interestScartatiRepoVect.resize(3);
	//for (uint32_t i = 0; i < 3; ++i)
	//{
		//interestScartatiRepoVect[i].resize(numSlot, 0);
	//}


	//std::string INTEREST_SCARTATI_FAILURE = "INTEREST/Interest_SCARTATI_FAILURE/InterestSCARTATI_FAILURE_Nodo_";
	//std::vector<std::vector<long double> > interestScartatiFailureVect; //= new std::vector<long double> (3,0);
	//interestScartatiFailureVect.resize(3);
	//for (uint32_t i = 0; i < 3; ++i)
	//{
		//interestScartatiFailureVect[i].resize(numSlot, 0);
	//}


	//std::string ENTRY_SCADUTE = "PIT/Entry_SCADUTE/EntrySCADUTE_Nodo_";
    //std::vector<std::vector<long double> > entryScaduteVect; //= new std::vector<long double> (3,0);
	//entryScaduteVect.resize(3);
	//for (uint32_t i = 0; i < 3; ++i)
	//{
		//entryScaduteVect[i].resize(numSlot, 0);
	//}


    // *** Output stream per i file che conterranno i valori medi dei parametri misurati per tutti e tre gli scenari

    std::vector<std::vector<long double> > distanceVect; //= new std::vector<long double> (3,0);
    	distanceVect.resize(numero_sim);
    	for (uint32_t i = 0; i < numero_sim; ++i)
    	{
    		distanceVect[i].resize(numSlot, 0);
    	}


    std::vector<std::vector<long double> > distanceVectFirst; //= new std::vector<long double> (3,0);
  	distanceVectFirst.resize(numero_sim);
   	for (uint32_t i = 0; i < numero_sim; ++i)
   	{
   		distanceVectFirst[i].resize(numSlot, 0);
   	}



    std::vector<std::string> * stringheOutput = new std::vector<std::string> (num_par_mis, "");
    //stringheOutput->operator [](0) = "MEDIE/Mean_DOWNLOAD_";
    stringheOutput->operator [](0) = "MEDIE/Mean_DISTANCE_";
    stringheOutput->operator [](1) = "MEDIE/Mean_HIT_";
    stringheOutput->operator [](2) = "MEDIE/Mean_DATA_INVIATI_";
    stringheOutput->operator [](3) = "MEDIE/Mean_DATA_RICEVUTI_";
    //stringheOutput->operator [](5) = "MEDIE/Mean_DATA_SCARTATI_";
    stringheOutput->operator [](4) = "MEDIE/Mean_INTEREST_INVIATI_";
    stringheOutput->operator [](5) = "MEDIE/Mean_INTEREST_RICEVUTI_";
    stringheOutput->operator [](6) = "MEDIE/Mean_INTEREST_AGGREGATI_";
    stringheOutput->operator [](7) = "MEDIE/Mean_INTEREST_ELIMINATI_";
    stringheOutput->operator [](8) = "MEDIE/Mean_INTEREST_RITRASMESSI_";
    //stringheOutput->operator [](11) = "MEDIE/Mean_INTEREST_SCARTATI_";
    //stringheOutput->operator [](12) = "MEDIE/Mean_INTEREST_SCARTATI_FAILURE_";
    //stringheOutput->operator [](13) = "MEDIE/Mean_EntrySCADUTE_";



    std::ifstream fin;
    std::ofstream fout;

    // SCENARI
    std::string SCENARIO; 
    std::string SCENARIO_output; 

    std::string SCENARIO_2 = ".BestRoute";
    std::string SCENARIO_2_output = "BestRoute";

    std::string SCENARIO_3 = ".BloomFilter";
    std::string SCENARIO_3_output = "BloomFilter";
    
    //std::string ALPHA = "_1";

    uint32_t numeroNodi = 22;
    // ALTRE COMPONENTI DEL NOME
    std::string NumSim;
    std::string Ratio = "0.001";
    std::string Seed = "1";
    std::string Run;

    std::stringstream ss;
    std::string line;
    const char* PATH;

    int ScenarioScelto;
    double Alpha;
    std::string ALPHA;
    
    std::cout << "Quale scenario vuoi elaborare? \n 1 - FLOODING\n 2 - BEST ROUTE\n 3 - BLOOM FILTER\n 4 - TUTTI\n";
    std::cin >> ScenarioScelto;

    std::cout << "Inserire valore di alpha:\n";
    std::cin >> Alpha;
    
    if(Alpha == 1)
	ALPHA = "_1";
    else if(Alpha == 1.2)
    	ALPHA = "_12";
    else {
	std::cout << "Inserire un valore previsto di Alpha (1 o 1.2)";
	exit(0);
    }

    switch(ScenarioScelto)
    {
       case(1):// FLOODING
    		
		SCENARIO = ".Flooding";
		SCENARIO_output = "Flooding";
		break;
       case(2):
		SCENARIO = ".BestRoute";
		SCENARIO_output = "BestRoute";
		break;
       case(3):
		SCENARIO = ".BloomFilter";
		SCENARIO_output = "BloomFilter";
		break;
    }


		for(uint32_t k = 1; k <= numero_sim; k++)  // Ciclo for per le tre simulazioni fatte per ogni scenario
		{

			// ***** DATA INVIATI ****
			for(uint32_t z=1; z <= numeroNodi; z++)
			{
			    ss << DATA_INVIATI << z << SCENARIO << "_" << k << "_" << Ratio << "_" << Seed << "_" << k << "_alpha" << ALPHA;
			    PATH = ss.str().c_str();
			    ss.str("");
			    fin.open(PATH);
			    if(!fin){
			      	std::cout << "\n File non presente!\n";
			      	fin.close();
			    }
			    else
			    {
			        fileAperti++;
			        if ((k==1 && z==18) || (k==1 && z==23) || (k==2 && z==15) || (k==2 && z==18) || (k==3 && z==17) || (k==3 && z==21) ||
				    (k==4 && z==11) || (k==4 && z==4)  || (k==5 && z==1)  || (k==5 && z==10) || (k==6 && z==18) || (k==6 && z==6)  ||
 				    (k==7 && z==7)  || (k==7 && z==5)  || (k==8 && z==4)  || (k==8 && z==8)  || (k==9 && z==20) || (k==9 && z==7)  ||
				    (k==10 && z==7) || (k==10 && z==11))
			        {
			        	fileApertiREPO++;
			        	repo = true;
			        }
			        else 
				   fileApertiCLIENT++;
				
				while(std::getline(fin, line))
			        {
			        	boost::char_separator<char> sep("\t");
			        	boost::tokenizer<boost::char_separator<char> > tokens(line, sep);
			        	boost::tokenizer<boost::char_separator<char> >::iterator it = tokens.begin();
			        	// Eliminazione del transitorio
			        	std::string strTrans = (*it);
			        	const char* valueTrans = strTrans.c_str();
			        	long double checkTrans = (long double) std::atol(valueTrans);
			        	uint32_t slot = floor(checkTrans/timeSlot);
			        	
			        	if(slot <= slotMax) {
		        		++it;
		        		++it;
		        		std::string stri = (*it);
		        		const char* value = stri.c_str();
		        		long double check = (long double) std::atoi(value);
		        		if(check == 1) {
		        			dataInviatiCACHE[k-1][slot]+=check;
		        			ValoriInseriti_sl->operator [](slot)++;
		        			ValoriInseriti++;
		        			ValoriInseritiCACHE_sl->operator [](slot)++;
		        			ValoriInseritiCACHE++;
					  	if (repo) {
					  		ValoriInseritiREPO++;
					  		ValoriInseritiREPO_sl->operator [](slot)++;
					  	}
					  	else {
					  		ValoriInseritiCLIENT++;
					  		ValoriInseritiCLIENT_sl->operator [](slot)++;
					  	}
					  	
		        		}
		        		else
		        		{
		        			ValoriInseriti_sl->operator [](slot)++;
		        			ValoriInseriti++;
		        			if (repo) {
					  		ValoriInseritiREPO++;
					  		ValoriInseritiREPO_sl->operator [](slot)++;
					  	}
					  	else {
					  		ValoriInseritiCLIENT++;
					  		ValoriInseritiCLIENT_sl->operator [](slot)++;
					  	}


		        		} }
		        		else
							break; 

		        	}
		          	fin.close();
			    }
			    repo = false;
			}

			
			for(uint32_t i = 0; i < numSlot; i++)
			{
				dataInviatiVect[k-1][i]= (long double)(ValoriInseriti_sl->operator [](i)/fileAperti);
				dataInviatiClientVect[k-1][i]= (long double)(ValoriInseritiCLIENT_sl->operator [](i)/fileApertiCLIENT);
				dataInviatiRepoVect[k-1][i]= (long double)(ValoriInseritiREPO_sl->operator [](i)/fileApertiREPO);
				dataInviatiCACHE[k-1][i] /= (long double)(fileAperti);
			}
//			std::cout << "File Aperti:\t" << fileAperti;
//			std::cout << "Valori Inseriti:\t" << ValoriInseriti;
			fileAperti = 0;
			fileApertiCLIENT = 0;
			fileApertiREPO = 0;
			ValoriInseriti = 0;
			ValoriInseritiCLIENT = 0;
			ValoriInseritiREPO = 0;
			for(uint32_t i = 0; i < numSlot; i++)
			{
				ValoriInseriti_sl->operator [](i) = 0;
				ValoriInseritiCLIENT_sl->operator [](i) = 0;
				ValoriInseritiREPO_sl->operator [](i) = 0;
	    			ValoriInseritiCACHE_sl->operator [](i) = 0;
			}




			// ***** DATA RICEVUTI ****			
			for(uint32_t z=1; z <= numeroNodi; z++)
			{
			    ss << DATA_RICEVUTI << z << SCENARIO << "_" << k << "_" << Ratio << "_" << Seed << "_" << k << "_alpha" << ALPHA;
			    
			    PATH = ss.str().c_str();
			    ss.str("");
			    fin.open(PATH);
			    if(!fin){
				std::cout << "\n File non presente!\n";
				fin.close();
			    }
			    else
			    {
			        fileAperti++;
			        if ((k==1 && z==18) || (k==1 && z==23) || (k==2 && z==15) || (k==2 && z==18) || (k==3 && z==17) || (k==3 && z==21) ||
				    (k==4 && z==11) || (k==4 && z==4)  || (k==5 && z==1)  || (k==5 && z==10) || (k==6 && z==18) || (k==6 && z==6)  ||
 				    (k==7 && z==7)  || (k==7 && z==5)  || (k==8 && z==4)  || (k==8 && z==8)  || (k==9 && z==20) || (k==9 && z==7)  ||
				    (k==10 && z==7) || (k==10 && z==11))
				{
			        	fileApertiREPO++;
			        	repo = true;
			        }
			        else 
				   	fileApertiCLIENT++;
				while(std::getline(fin, line))
				{
					boost::char_separator<char> sep("\t");
					boost::tokenizer<boost::char_separator<char> > tokens(line, sep);
					boost::tokenizer<boost::char_separator<char> >::iterator it = tokens.begin();
					// Eliminazione del transitorio
					std::string strTrans = (*it);
					const char* valueTrans = strTrans.c_str();
					long double checkTrans = (long double) std::atol(valueTrans);
			        	uint32_t slot = floor(checkTrans/timeSlot);
			        	
			        	if(slot <= slotMax) 
					{
				  		ValoriInseriti++;
		        			ValoriInseriti_sl->operator [](slot)++;
				  		if (!repo){
				  			ValoriInseritiCLIENT++;
				  			ValoriInseritiCLIENT_sl->operator [](slot)++;
				  		}
				  	 }
				  	 else
				        	break;

					}
					fin.close();
				}
				repo = false;
			}
			for(uint32_t i = 0; i < numSlot; i++)
			{
				dataRicevutiVect[k-1][i] = (long double)(ValoriInseriti_sl->operator [](i)/fileAperti);
				dataRicevutiClientVect[k-1][i] = (long double)(ValoriInseritiCLIENT_sl->operator [](i) / fileApertiCLIENT);
			}

//			std::cout << "File Aperti:\t" << fileAperti;
//          std::cout << "Valori Inseriti:\t" << ValoriInseriti;

			fileAperti = 0;
			fileApertiCLIENT = 0;
			fileApertiREPO = 0;

			ValoriInseriti = 0;
			ValoriInseritiCLIENT = 0;
			for(uint32_t i = 0; i < numSlot; i++)
			{
				ValoriInseriti_sl->operator [](i) = 0;
    				ValoriInseritiCLIENT_sl->operator [](i) = 0;
    			}
			
			
			// ***** DATA RICEVUTI APP ****
			for(uint32_t z=1; z <= numeroNodi; z++)
			{
			    ss << DATA_RICEVUTI_APP << z << SCENARIO << "_" << k << "_" << Ratio << "_" << Seed << "_" << k << "_alpha" << ALPHA;
			    PATH = ss.str().c_str();
			    ss.str("");
			    fin.open(PATH);
			    if(!fin){
				std::cout << "\n File non presente!\n";
				fin.close();
			    }
			    else
			    {
			        fileAperti++;
			        if ((k==1 && z==18) || (k==1 && z==23) || (k==2 && z==15) || (k==2 && z==18) || (k==3 && z==17) || (k==3 && z==21) ||
				    (k==4 && z==11) || (k==4 && z==4)  || (k==5 && z==1)  || (k==5 && z==10) || (k==6 && z==18) || (k==6 && z==6)  ||
 				    (k==7 && z==7)  || (k==7 && z==5)  || (k==8 && z==4)  || (k==8 && z==8)  || (k==9 && z==20) || (k==9 && z==7)  ||
				    (k==10 && z==7) || (k==10 && z==11))
					{
			        	fileApertiREPO++;
			        	continue;
			        	
			        }
					else
						fileApertiCLIENT++;
					while(std::getline(fin, line))
					{
					boost::char_separator<char> sep("\t");
					boost::tokenizer<boost::char_separator<char> > tokens(line, sep);
					boost::tokenizer<boost::char_separator<char> >::iterator it = tokens.begin();
					// Eliminazione del transitorio
					std::string strTrans = (*it);
					const char* valueTrans = strTrans.c_str();
					long double checkTrans = (long double) std::atol(valueTrans);
			        	uint32_t slot = floor(checkTrans/timeSlot);
			        	
			        	if(slot <= slotMax) {
				  		ValoriInseritiCLIENT++;
	        				ValoriInseritiCLIENT_sl->operator [](slot)++;
					}
				} 
				fin.close();
			    }
				
			}
			for(uint32_t i = 0; i < numSlot; i++)
			{
				dataRicevutiAppVect[k-1][i] = (long double)(ValoriInseritiCLIENT_sl->operator [](i)/fileApertiCLIENT);
			}

			fileAperti = 0;
			fileApertiCLIENT = 0;
			fileApertiREPO = 0;

			ValoriInseritiCLIENT = 0;
			
			for(uint32_t i = 0; i < numSlot; i++)
			{
				ValoriInseritiCLIENT_sl->operator [](i) = 0;
    			}



			// ******  INTEREST INVIATI  ******
			

			for(uint32_t z=1; z <= numeroNodi; z++)
			{
			    ss << INTEREST_INVIATI << z << SCENARIO << "_" << k << "_" << Ratio << "_" << Seed << "_" << k << "_alpha" << ALPHA;
			    PATH = ss.str().c_str();
			    ss.str("");
			    fin.open(PATH);
			    if(!fin){
				std::cout << "\n File non presente!\n";
				fin.close();
			    }
			    else
			    {
				fileAperti++;
			        if ((k==1 && z==18) || (k==1 && z==23) || (k==2 && z==15) || (k==2 && z==18) || (k==3 && z==17) || (k==3 && z==21) ||
				    (k==4 && z==11) || (k==4 && z==4)  || (k==5 && z==1)  || (k==5 && z==10) || (k==6 && z==18) || (k==6 && z==6)  ||
 				    (k==7 && z==7)  || (k==7 && z==5)  || (k==8 && z==4)  || (k==8 && z==8)  || (k==9 && z==20) || (k==9 && z==7)  ||
				    (k==10 && z==7) || (k==10 && z==11))
				{
			        	fileApertiREPO++;
			        	repo = true;
			        }
			        else
				   fileApertiCLIENT++;
				while(std::getline(fin, line))
				{
					boost::char_separator<char> sep("\t");
					boost::tokenizer<boost::char_separator<char> > tokens(line, sep);
					boost::tokenizer<boost::char_separator<char> >::iterator it = tokens.begin();
					// Eliminazione del transitorio
					std::string strTrans = (*it);
					const char* valueTrans = strTrans.c_str();
					long double checkTrans = (long double) std::atol(valueTrans);
			        	uint32_t slot = floor(checkTrans/timeSlot);
			        	
			        	if(slot <= slotMax) 
					{
				  		ValoriInseriti++;
		        			ValoriInseriti_sl->operator [](slot)++;
				  		if (!repo){
				  			ValoriInseritiCLIENT++;
				  			ValoriInseritiCLIENT_sl->operator [](slot)++;
				  		}
				  	}
				  	else
						break;
				}
				fin.close();
			     }
			     repo = false;
			}
			for(uint32_t i = 0; i < numSlot; i++)
			{
				interestInviatiVect[k-1][i] = (long double)(ValoriInseriti_sl->operator [](i)/fileAperti);
				interestInviatiClientVect[k-1][i] = (long double)(ValoriInseritiCLIENT_sl->operator [](i) / fileApertiCLIENT);
				
			}

			//std::cout << "File Aperti:\t" << fileAperti;
                        //std::cout << "Valori Inseriti:\t" << ValoriInseriti;

			fileAperti = 0;
			fileApertiCLIENT = 0;
			fileApertiREPO = 0;

			ValoriInseriti = 0;
			ValoriInseritiCLIENT = 0;
			
			for(uint32_t i = 0; i < numSlot; i++)
			{
				ValoriInseriti_sl->operator [](i) = 0;
    				ValoriInseritiCLIENT_sl->operator [](i) = 0;
    				ValoriInseritiCORE_sl->operator [](i) = 0;
			}
			
			
			// ******  INTEREST INVIATI APP ******
			for(uint32_t z=1; z <= numeroNodi; z++)
			{
			    ss << INTEREST_INVIATI_APP << z << SCENARIO << "_" << k << "_" << Ratio << "_" << Seed << "_" << k << "_alpha" << ALPHA;
			    PATH = ss.str().c_str();
			    ss.str("");
			    fin.open(PATH);
			    if(!fin){
				std::cout << "\n File non presente!\n";
				fin.close();
			    }
			    else
			    {
			       	fileApertiCLIENT++;
				while(std::getline(fin, line))
				{
					boost::char_separator<char> sep("\t");
					boost::tokenizer<boost::char_separator<char> > tokens(line, sep);
					boost::tokenizer<boost::char_separator<char> >::iterator it = tokens.begin();
					// Eliminazione del transitorio
					std::string strTrans = (*it);
					const char* valueTrans = strTrans.c_str();
					long double checkTrans = (long double) std::atol(valueTrans);
			        	uint32_t slot = floor(checkTrans/timeSlot);
			        	
			        	if(slot <= slotMax) {
				  		ValoriInseritiCLIENT++;
	        				ValoriInseritiCLIENT_sl->operator [](slot)++;
					}
				} 
				fin.close();
			    }
				
			}
			for(uint32_t i = 0; i < numSlot; i++)
			{
				interestInviatiAppVect[k-1][i] = (long double)(ValoriInseritiCLIENT_sl->operator [](i)/fileApertiCLIENT);
			}

			fileAperti = 0;
			fileApertiCLIENT = 0;
			
			ValoriInseritiCLIENT = 0;
			
			for(uint32_t i = 0; i < numSlot; i++)
			{
				ValoriInseritiCLIENT_sl->operator [](i) = 0;
    			}


			// ******  INTEREST RICEVUTI ******
			for(uint32_t z=1; z <= numeroNodi; z++)
			{
			    ss << INTEREST_RICEVUTI << z << SCENARIO << "_" << k << "_" << Ratio << "_" << Seed << "_" << k << "_alpha" << ALPHA;
			    PATH = ss.str().c_str();
			    ss.str("");
			    fin.open(PATH);
			    if(!fin){
				std::cout << "\n File non presente!\n";
				fin.close();
			    }
			    else
			    {
				fileAperti++;
			        if ((k==1 && z==18) || (k==1 && z==23) || (k==2 && z==15) || (k==2 && z==18) || (k==3 && z==17) || (k==3 && z==21) ||
				    (k==4 && z==11) || (k==4 && z==4)  || (k==5 && z==1)  || (k==5 && z==10) || (k==6 && z==18) || (k==6 && z==6)  ||
 				    (k==7 && z==7)  || (k==7 && z==5)  || (k==8 && z==4)  || (k==8 && z==8)  || (k==9 && z==20) || (k==9 && z==7)  ||
				    (k==10 && z==7) || (k==10 && z==11))
				{
			        	fileApertiREPO++;
			        	repo = true;
			        }
			        else
					fileApertiCLIENT++;
				while(std::getline(fin, line))
				{
					boost::char_separator<char> sep("\t");
					boost::tokenizer<boost::char_separator<char> > tokens(line, sep);
					boost::tokenizer<boost::char_separator<char> >::iterator it = tokens.begin();
					// Eliminazione del transitorio
					std::string strTrans = (*it);
					const char* valueTrans = strTrans.c_str();
					long double checkTrans = (long double) std::atol(valueTrans);
			        	uint32_t slot = floor(checkTrans/timeSlot);
			        	
			        	if(slot <= slotMax) {
				  		ValoriInseriti++;
	        				ValoriInseriti_sl->operator [](slot)++;
				  		if (repo) {
				  			ValoriInseritiREPO++;
				  			ValoriInseritiREPO_sl->operator [](slot)++;
				  		}
				  		else if (!repo){
				  			ValoriInseritiCLIENT++;
				  			ValoriInseritiCLIENT_sl->operator [](slot)++;
				  		}
				  	}
				  	else
						break;

				} 
				fin.close();
			    }
			    repo = false;
			}
			for(uint32_t i = 0; i < numSlot; i++)
			{
				interestRicevutiVect[k-1][i] = (long double)(ValoriInseriti_sl->operator [](i)/fileAperti);
				interestRicevutiClientVect[k-1][i] = (long double)(ValoriInseritiCLIENT_sl->operator [](i)/fileApertiCLIENT);
				interestRicevutiRepoVect[k-1][i] = (long double)(ValoriInseritiREPO_sl->operator [](i)/fileApertiREPO);
			}

			fileAperti = 0;
			fileApertiREPO = 0;
			fileApertiCLIENT = 0;
			ValoriInseriti = 0;
			ValoriInseritiCLIENT = 0;
			ValoriInseritiREPO = 0;

			for(uint32_t i = 0; i < numSlot; i++)
			{
				ValoriInseriti_sl->operator [](i) = 0;
				ValoriInseritiCLIENT_sl->operator [](i) = 0;
				ValoriInseritiREPO_sl->operator [](i) = 0;
			}


			// ******  INTEREST RITRASMESSI ******
			for(uint32_t z=1; z <= numeroNodi; z++)
			{
			    ss << INTEREST_RITRASMESSI << z << SCENARIO << "_" << k << "_" << Ratio << "_" << Seed << "_" << k << "_alpha" << ALPHA;
			    PATH = ss.str().c_str();
			    ss.str("");
			    fin.open(PATH);
			    if(!fin){
				std::cout << "\n File non presente!\n";
				fin.close();
			    }
			    else
			    {
				fileApertiCLIENT++;

				while(std::getline(fin, line))
				{
					boost::char_separator<char> sep("\t");
					boost::tokenizer<boost::char_separator<char> > tokens(line, sep);
					boost::tokenizer<boost::char_separator<char> >::iterator it = tokens.begin();
					// Eliminazione del transitorio
					std::string strTrans = (*it);
					const char* valueTrans = strTrans.c_str();
					long double checkTrans = (long double) std::atol(valueTrans);
			        	uint32_t slot = floor(checkTrans/timeSlot);
			        	
			        	if(slot <= slotMax) {
				  		ValoriInseriti++;
	        				ValoriInseriti_sl->operator [](slot)++;
					}
					else
						break;
				}
				fin.close();
			    }
			}
			for(uint32_t i = 0; i < numSlot; i++)
			{
				interestRitrasmessiVect[k-1][i] = (long double)(ValoriInseriti_sl->operator [](i)/fileApertiCLIENT);
			}

			fileAperti = 0;
			fileApertiCLIENT = 0;
			
			ValoriInseriti = 0;
			for(uint32_t i = 0; i < numSlot; i++)
			{
				ValoriInseriti_sl->operator [](i) = 0;
			}


			

			// ******  INTEREST ELIMINATI ******
			for(uint32_t z=1; z <= numeroNodi; z++)
			{
			    ss << INTEREST_ELIMINATI << z << SCENARIO << "_" << k << "_" << Ratio << "_" << Seed << "_" << k << "_alpha" << ALPHA;
			    PATH = ss.str().c_str();
			    ss.str("");
			    fin.open(PATH);
			    if(!fin){
			  	std::cout << "\n File non presente!\n";
			  	fin.close();
			    }
			    else
			    {
			        fileAperti++;
			  	while(std::getline(fin, line))
		         	{
					boost::char_separator<char> sep("\t");
					boost::tokenizer<boost::char_separator<char> > tokens(line, sep);
					boost::tokenizer<boost::char_separator<char> >::iterator it = tokens.begin();
					// Eliminazione del transitorio
				  		
     			  		std::string strTrans_1 = (*it);
					const char* valueTrans_1 = strTrans_1.c_str();
					long double checkTrans_1 = (long double) std::atol(valueTrans_1);
			        	uint32_t slot_1 = floor(checkTrans_1/timeSlot);
			        	
			        	if(slot_1 <= slotMax) {				  		
				  		++it;
				  		++it;
				  		std::string strTrans = (*it);
				  		const char* valueTrans = strTrans.c_str();
				  		long double checkTrans = (long double) std::atol(valueTrans);
			        		uint32_t slot = floor(checkTrans/timeSlot);
				  		ValoriInseriti++;
	        				ValoriInseriti_sl->operator [](slot)++;
					}
					else
						break;
					}
				  	fin.close();
			    }
			}
			for(uint32_t i = 0; i < numSlot; i++)
			{
				interestEliminatiVect[k-1][i] = (long double)(ValoriInseriti_sl->operator [](i)/fileAperti);
			}
			fileAperti = 0;
			ValoriInseriti = 0;
			for(uint32_t i = 0; i < numSlot; i++)
			{
				ValoriInseriti_sl->operator [](i) = 0;
			}
			
			
			// ******  INTEREST ELIMINATI FILE ******
			for(uint32_t z=1; z <= numeroNodi; z++)
			{
			    ss << INTEREST_ELIMINATI_FILE << z << SCENARIO << "_" << k << "_" << Ratio << "_" << Seed << "_" << k << "_alpha" << ALPHA;
			    PATH = ss.str().c_str();
			    ss.str("");
			    fin.open(PATH);
			    if(!fin){
				std::cout << "\n File non presente!\n";
				fin.close();
			    }
			    else
			    {
			        fileAperti++;
			   	while(std::getline(fin, line))
		         	{
			  		boost::char_separator<char> sep("\t");
			  		boost::tokenizer<boost::char_separator<char> > tokens(line, sep);
			  		boost::tokenizer<boost::char_separator<char> >::iterator it = tokens.begin();
			  		//++it;
			  		//++it;
			  		std::string strTrans = (*it);
			  		const char* valueTrans = strTrans.c_str();
			  		long double checkTrans = (long double) std::atol(valueTrans);
			        	uint32_t slot = floor(checkTrans/timeSlot);
			        	
			        	if(slot <= slotMax) {
				  		ValoriInseriti++;
		        			ValoriInseriti_sl->operator [](slot)++;
					}
					else
						break;
				}
				fin.close();
			    }
			}
			for(uint32_t i = 0; i < numSlot; i++)
			{
				interestEliminatiFileVect[k-1][i] = (long double)(ValoriInseriti_sl->operator [](i)/fileAperti);
			}
			fileAperti = 0;
			ValoriInseriti = 0;
			for(uint32_t i = 0; i < numSlot; i++)
			{
				ValoriInseriti_sl->operator [](i) = 0;
			}




			// ******  INTEREST AGGREGATI ******
			for(uint32_t z=1; z <= numeroNodi; z++)
			{
			    ss << INTEREST_AGGREGATI << z << SCENARIO << "_" << k << "_" << Ratio << "_" << Seed << "_" << k << "_alpha" << ALPHA;
			    PATH = ss.str().c_str();
			    ss.str("");
			    fin.open(PATH);
			    if(!fin){
				std::cout << "\n File non presente!\n";
				fin.close();
			    }
			    else
			    {
				fileAperti++;
				while(std::getline(fin, line))
				{
					boost::char_separator<char> sep("\t");
					boost::tokenizer<boost::char_separator<char> > tokens(line, sep);
					boost::tokenizer<boost::char_separator<char> >::iterator it = tokens.begin();
					// Eliminazione del transitorio
					std::string strTrans = (*it);
					const char* valueTrans = strTrans.c_str();
					long double checkTrans = (long double) std::atol(valueTrans);
			        	uint32_t slot = floor(checkTrans/timeSlot);
			        	
			        	if(slot <= slotMax) {
				  		ValoriInseriti++;
	        				ValoriInseriti_sl->operator [](slot)++;
					}
					else
						break;
				}
				fin.close();
			    }
			}
			for(uint32_t i = 0; i < numSlot; i++)
			{
				interestAggregatiVect[k-1][i] = (long double)(ValoriInseriti_sl->operator [](i)/fileAperti);
			}
			fileAperti = 0;
			ValoriInseriti = 0;
			for(uint32_t i = 0; i < numSlot; i++)
			{
				ValoriInseriti_sl->operator [](i) = 0;
			}




			// ***** DOWNLOAD FILE *****
			for(uint32_t z=1; z <= numeroNodi; z++)
			{
			    ss << DOWNLOAD_FILE << z << SCENARIO << "_" << k << "_" << Ratio << "_" << Seed << "_" << k << "_alpha" << ALPHA;
			    PATH = ss.str().c_str();
			    ss.str("");
			    fin.open(PATH);
			    if(!fin){
			  	std::cout << "\n File non presente!\n";
			  	fin.close();
			    }
			    else
			    {
				fileAperti++;
				while(std::getline(fin, line))
				{
					boost::char_separator<char> sep("\t");
			        	boost::tokenizer<boost::char_separator<char> > tokens(line, sep);
			        	boost::tokenizer<boost::char_separator<char> >::iterator it = tokens.begin();
			        	// Eliminazione del transitorio
		        		++it;
		        		++it;   // Vado a posizionarmi in corrispondenza del tempo di invio
			        	std::string strTrans = (*it);
			        	const char* valueTrans = strTrans.c_str();
			        	long double checkTrans = (long double) std::atol(valueTrans);
			        	uint32_t slot = floor(checkTrans/timeSlot);
			        	
			        	if(slot <= slotMax) {
						++it;  // Mi posiziono in corrispondenza del tempo di download
			        		std::string stri = (*it);
			        		const char* value = stri.c_str();
			        		long double check = (long double) std::atol(value);
			        		if(check > 0) //&& check < 5000000000)
			        		{
			        			downloadFileVect[k-1][slot] += check;
			        		
			        			++it;    // Mi posiziono in corrispondenza della distanza media di recupero del file
			        			std::string striDist = (*it);
			        			const char* valueDist = striDist.c_str();
			        			long double checkDist = (long double) (std::atoi(valueDist)-1);      // Viene tolto 1 perchè l'HC viene incrementato sia dalle net-face che dalle app-face
			        			distanceVect[k-1][slot] += checkDist;
		
			        			ValoriInseriti++;
		        				ValoriInseriti_sl->operator [](slot)++;
			        		}
			        		else
			        		{
							ValoriInseritiLOCAL++;
							ValoriInseritiLOCAL_sl->operator [](slot)++;
						}
			        	
			        	}
			        	else
						break;

				  }
				  fin.close();
				}
			}
			for(uint32_t i = 0; i < numSlot; i++)
			{
				downloadFileVect[k-1][i] /= (long double)(ValoriInseriti_sl->operator [](i));
				downloadFileAllLocalVect[k-1][i] = (long double)(ValoriInseritiLOCAL_sl->operator [](i)/fileAperti);
				distanceVect[k-1][i] /= (long double)(ValoriInseriti_sl->operator [](i));
			}
			
			fileAperti = 0;
			ValoriInseriti = 0;
			ValoriInseritiLOCAL = 0;
			for(uint32_t i = 0; i < numSlot; i++)
			{
				ValoriInseriti_sl->operator [](i) = 0;
				ValoriInseritiLOCAL_sl->operator [](i) = 0;
			}


			// ***** DOWNLOAD FIRST CHUNK *****
			for(uint32_t z=1; z <= numeroNodi; z++)
			{
			    ss << DOWNLOAD_FIRST << z << SCENARIO << "_" << k << "_" << Ratio << "_" << Seed << "_" << k << "_alpha" << ALPHA;
			    PATH = ss.str().c_str();
			    ss.str("");
			    fin.open(PATH);
			    if(!fin){
				std::cout << "\n File non presente!\n";
				fin.close();
			    }
			    else
			    {
				fileAperti++;
				while(std::getline(fin, line))
				{
					boost::char_separator<char> sep("\t");
			        	boost::tokenizer<boost::char_separator<char> > tokens(line, sep);
			        	boost::tokenizer<boost::char_separator<char> >::iterator it = tokens.begin();
			        	// Eliminazione del transitorio
		        		++it;
		        		++it;   // Vado a posizionarmi in corrispondenza del tempo di invio
			        	std::string strTrans = (*it);
			        	const char* valueTrans = strTrans.c_str();
			        	long double checkTrans = (long double) std::atol(valueTrans);
			        	uint32_t slot = floor(checkTrans/timeSlot);
			        	
			        	if(slot <= slotMax) {
						++it;  // Mi posiziono in corrispondenza del tempo di download
			        		std::string stri = (*it);
			        		const char* value = stri.c_str();
			        		long double check = (long double) std::atol(value);
			        		if(check > 0) //&& check < 5000000000)
			        		{
			        			downloadFirstVect[k-1][slot] += check;
			        		
			        			++it;    // Mi posiziono in corrispondenza della distanza media di recupero del file
			        			std::string striDist = (*it);
			        			const char* valueDist = striDist.c_str();
			        			long double checkDist = (long double) (std::atoi(valueDist)-1);      // Viene tolto 1 perchè l'HC viene incrementato sia dalle net-face che dalle app-face
			        			distanceVectFirst[k-1][slot] += checkDist;

			        			ValoriInseriti++;
		        				ValoriInseriti_sl->operator [](slot)++;
			        		}
			        		else
			        		{
							ValoriInseritiLOCAL++;
							ValoriInseritiLOCAL_sl->operator [](slot)++;
						}
			        	
			        	}
			        	else
						break;

				    }
				  	fin.close();
				}
			}
			for(uint32_t i = 0; i < numSlot; i++)
			{
				downloadFirstVect[k-1][i] /= (long double)(ValoriInseriti_sl->operator [](i));
				downloadFirstAllLocalVect[k-1][i] = (long double)(ValoriInseritiLOCAL_sl->operator [](i)/fileAperti);
				distanceVectFirst[k-1][i] /= (long double)(ValoriInseriti_sl->operator [](i));
			}
			
			fileAperti = 0;
			ValoriInseriti = 0;
			ValoriInseritiLOCAL = 0;
			for(uint32_t i = 0; i < numSlot; i++)
			{
				ValoriInseriti_sl->operator [](i) = 0;
				ValoriInseritiLOCAL_sl->operator [](i) = 0;
			}



	}

        // ***** CALCOLO DEI VALORI MEDI *****

        for(uint32_t t = 0; t <= slotMax; t++)
        {
        	for(uint32_t j = 0; j < numero_sim; j++)
        	{
        	    parametriMisurati[0][j][t] = distanceVect[j][t];
        	    parametriMisurati[1][j][t] = (long double)((dataInviatiCACHE[j][t])/(interestRicevutiVect[j][t]));   // Hit Ratio
        	    parametriMisurati[2][j][t] = dataInviatiVect[j][t];
        	    parametriMisurati[3][j][t] = dataRicevutiVect[j][t];
        	    //parametriMisurati[5][j][t] = dataScartatiVect[j][t];
        	    parametriMisurati[4][j][t] = interestInviatiVect[j][t];
        	    parametriMisurati[5][j][t] = interestRicevutiVect[j][t];
        	    parametriMisurati[6][j][t] = interestAggregatiVect[j][t];
        	    parametriMisurati[7][j][t] = interestEliminatiVect[j][t];
        	    parametriMisurati[8][j][t] = interestRitrasmessiVect[j][t];
        	    //parametriMisurati[11][t] = interestScartatiVect[j][t];
        	    //parametriMisurati[12][t] = interestScartatiFailureVect[j][t];
        	    //parametriMisurati[13][t] = entryScaduteVect[j][t];
		}
	}
        	    
        	    
        // **** SCRITTURA SU FILE DEI RISULTATI *****
        for(uint32_t z = 0; z < num_par_mis; z++)
        {
        	ss << stringheOutput->operator [](z) << SCENARIO_output << ALPHA;
        	//std::cout << "Nome file:\t" << stringheOutput->operator[](z);
        	PATH = ss.str().c_str();
        	ss.str("");
        	fout.open(PATH);
        	if(!fout) {
        		std::cout << "\n Impossibile Aprire il File";
        		exit(1);
        	}
        	else
        	{
        		for(uint32_t t = 0; t < slotMax; t++)
        		{
				for(uint32_t j = 0; j < numero_sim; j++) 
				{
					if (j == numero_sim-1)
						fout << parametriMisurati[z][j][t] << "\n";
					else
						fout << parametriMisurati[z][j][t] << "\t";
				}
        		}
        	}
        	fout.close();
        }

        // *** DATA RICEVUTI CLIENT ****
        ss << stringheOutput->operator [](3) << SCENARIO_output << "_" << "CLIENT" << ALPHA;
        PATH = ss.str().c_str();
        ss.str("");
        fout.open(PATH);
       	if(!fout) {
       		std::cout << "\n Impossibile Aprire il File";
       		exit(1);
       	}
       	else
       	{
       		for(uint32_t t = 0; t < slotMax; t++)
        	{
			for(uint32_t j = 0; j < numero_sim; j++) 
			{
				if (j == numero_sim-1)
					fout << dataRicevutiClientVect[j][t] << "\n";
				else
					fout << dataRicevutiClientVect[j][t] << "\t";
			}
        	}

       	}
       	fout.close();


       	// *** DATA RICEVUTI APP ****
        ss << stringheOutput->operator [](3) << SCENARIO_output << "_" << "APP" << ALPHA;
        PATH = ss.str().c_str();
        ss.str("");
        fout.open(PATH);
       	if(!fout) {
       		std::cout << "\n Impossibile Aprire il File";
       		exit(1);
       	}
       	else
       	{
        	for(uint32_t t = 0; t < slotMax; t++)
        	{
			for(uint32_t j = 0; j < numero_sim; j++) 
			{
				if (j == numero_sim-1)
					fout << dataRicevutiAppVect[j][t] << "\n";
				else
					fout << dataRicevutiAppVect[j][t] << "\t";
			}
        	}
       	}
       	fout.close();



        // *** DATA INVIATI CLIENT ****
        ss << stringheOutput->operator [](2) << SCENARIO_output << "_" << "CLIENT" << ALPHA;
        PATH = ss.str().c_str();
        ss.str("");
        fout.open(PATH);
       	if(!fout) {
       		std::cout << "\n Impossibile Aprire il File";
       		exit(1);
       	}
       	else
       	{
        	for(uint32_t t = 0; t < slotMax; t++)
        	{
			for(uint32_t j = 0; j < numero_sim; j++) 
			{
				if (j == numero_sim-1)
					fout << dataInviatiClientVect[j][t] << "\n";
				else
					fout << dataInviatiClientVect[j][t] << "\t";
			}
        	}
       	}
       	fout.close();


        // *** DATA INVIATI REPO ****
        ss << stringheOutput->operator [](2) << SCENARIO_output << "_" << "REPO" << ALPHA;
        PATH = ss.str().c_str();
        ss.str("");
        fout.open(PATH);
       	if(!fout) {
       		std::cout << "\n Impossibile Aprire il File";
       		exit(1);
       	}
       	else
       	{
        	for(uint32_t t = 0; t < slotMax; t++)
        	{
			for(uint32_t j = 0; j < numero_sim; j++) 
			{
				if (j == numero_sim-1)
					fout << dataInviatiRepoVect[j][t] << "\n";
				else
					fout << dataInviatiRepoVect[j][t] << "\t";
			}
        	}
       	}
       	fout.close();

        
       	
	// *** INTEREST INVIATI CLIENT ****
        ss << stringheOutput->operator [](4) << SCENARIO_output << "_" << "CLIENT" << ALPHA;
        PATH = ss.str().c_str();
        ss.str("");
        fout.open(PATH);
       	if(!fout) {
       		std::cout << "\n Impossibile Aprire il File";
       		exit(1);
       	}
       	else
       	{
        	for(uint32_t t = 0; t < slotMax; t++)
        	{
			for(uint32_t j = 0; j < numero_sim; j++) 
			{
				if (j == numero_sim-1)
					fout << interestInviatiClientVect[j][t] << "\n";
				else
					fout << interestInviatiClientVect[j][t] << "\t";
			}
        	}
       	}
       	fout.close();

      	
       	// *** INTEREST INVIATI APP ****
        ss << stringheOutput->operator [](4) << SCENARIO_output << "_" << "APP" << ALPHA;
        PATH = ss.str().c_str();
        ss.str("");
        fout.open(PATH);
       	if(!fout) {
       		std::cout << "\n Impossibile Aprire il File";
       		exit(1);
       	}
       	else
       	{
        	for(uint32_t t = 0; t < slotMax; t++)
        	{
			for(uint32_t j = 0; j < numero_sim; j++) 
			{
				if (j == numero_sim-1)
					fout << interestInviatiAppVect[j][t] << "\n";
				else
					fout << interestInviatiAppVect[j][t] << "\t";
			}
        	}
       	}
       	fout.close();

       	
       	// *** INTEREST RICEVUTI CLIENT ****
        ss << stringheOutput->operator [](5) << SCENARIO_output << "_" << "CLIENT" << ALPHA;
        PATH = ss.str().c_str();
        ss.str("");
        fout.open(PATH);
       	if(!fout) {
       		std::cout << "\n Impossibile Aprire il File";
       		exit(1);
       	}
       	else
       	{
        	for(uint32_t t = 0; t < slotMax; t++)
        	{
			for(uint32_t j = 0; j < numero_sim; j++) 
			{
				if (j == numero_sim-1)
					fout << interestRicevutiClientVect[j][t] << "\n";
				else
					fout << interestRicevutiClientVect[j][t] << "\t";
			}
        	}
       	}
       	fout.close();

        
	// *** INTEREST RICEVUTI REPO ****
        ss << stringheOutput->operator [](5) << SCENARIO_output << "_" << "REPO" << ALPHA;
        PATH = ss.str().c_str();
        ss.str("");
        fout.open(PATH);
       	if(!fout) {
       		std::cout << "\n Impossibile Aprire il File";
       		exit(1);
       	}
       	else
       	{
        	for(uint32_t t = 0; t < slotMax; t++)
        	{
			for(uint32_t j = 0; j < numero_sim; j++) 
			{
				if (j == numero_sim-1)
					fout << interestRicevutiRepoVect[j][t] << "\n";
				else
					fout << interestRicevutiRepoVect[j][t] << "\t";
			}
        	}
        }
       	fout.close();

       	
       	// *** DOWNLOAD TIME FILE ****
       	ss << "MEDIE/Mean_DOWNLOAD_FILE_" << SCENARIO_output << ALPHA;
        PATH = ss.str().c_str();
        ss.str("");
        fout.open(PATH);
       	if(!fout) {
       		std::cout << "\n Impossibile Aprire il File";
       		exit(1);
       	}
       	else
       	{
        	for(uint32_t t = 0; t < slotMax; t++)
        	{
			for(uint32_t j = 0; j < numero_sim; j++) 
			{
				if (j == numero_sim-1)
					fout << downloadFileVect[j][t] << "\n";
				else
					fout << downloadFileVect[j][t] << "\t";
			}
        	}
       	}
       	fout.close();
       	
       	// *** DOWNLOAD FILE ALL LOCAL ****
       	ss << "MEDIE/Mean_DOWNLOAD_FILE_ALL_LOCAL_" << SCENARIO_output << ALPHA;
        PATH = ss.str().c_str();
        ss.str("");
        fout.open(PATH);
       	if(!fout) {
       		std::cout << "\n Impossibile Aprire il File";
       		exit(1);
       	}
       	else
       	{
        	for(uint32_t t = 0; t < slotMax; t++)
        	{
			for(uint32_t j = 0; j < numero_sim; j++) 
			{
				if (j == numero_sim-1)
					fout << downloadFileAllLocalVect[j][t] << "\n";
				else
					fout << downloadFileAllLocalVect[j][t] << "\t";
			}
        	}
       	}
       	fout.close();

       	
       	// *** DOWNLOAD TIME FIRST ****
       	ss << "MEDIE/Mean_DOWNLOAD_FIRST_" << SCENARIO_output << ALPHA;
        PATH = ss.str().c_str();
        ss.str("");
        fout.open(PATH);
       	if(!fout) {
       		std::cout << "\n Impossibile Aprire il File";
       		exit(1);
       	}
       	else
       	{
        	for(uint32_t t = 0; t < slotMax; t++)
        	{
			for(uint32_t j = 0; j < numero_sim; j++) 
			{
				if (j == numero_sim-1)
					fout << downloadFirstVect[j][t] << "\n";
				else
					fout << downloadFirstVect[j][t] << "\t";
			}
        	}
       	}
       	fout.close();
       	
       	
       	// *** DOWNLOAD FIRST ALL LOCAL****
       	ss << "MEDIE/Mean_DOWNLOAD_FIRST_ALL_LOCAL_" << SCENARIO_output << ALPHA;
        PATH = ss.str().c_str();
        ss.str("");
        fout.open(PATH);
       	if(!fout) {
       		std::cout << "\n Impossibile Aprire il File";
       		exit(1);
       	}
       	else
       	{
        	for(uint32_t t = 0; t < slotMax; t++)
        	{
			for(uint32_t j = 0; j < numero_sim; j++) 
			{
				if (j == numero_sim-1)
					fout << downloadFirstAllLocalVect[j][t] << "\n";
				else
					fout << downloadFirstAllLocalVect[j][t] << "\t";
			}
        	}
       	}
       	fout.close();
       	
       	
       	// *** DISTANCE FIRST****
       	ss << "MEDIE/Mean_DISTANCE_FIRST_" << SCENARIO_output << ALPHA;
        PATH = ss.str().c_str();
        ss.str("");
        fout.open(PATH);
       	if(!fout) {
       		std::cout << "\n Impossibile Aprire il File";
       		exit(1);
       	}
       	else
       	{
        	for(uint32_t t = 0; t < slotMax; t++)
        	{
			for(uint32_t j = 0; j < numero_sim; j++) 
			{
				if (j == numero_sim-1)
					fout << distanceVectFirst[j][t] << "\n";
				else
					fout << distanceVectFirst[j][t] << "\t";
			}
        	}
       	}
       	fout.close();

       	
       	// *** INTEREST ELIMINATI FILE ****
       	ss << stringheOutput->operator [](7) << "FILE_" << SCENARIO_output << ALPHA;
        PATH = ss.str().c_str();
        ss.str("");
        fout.open(PATH);
       	if(!fout) {
       		std::cout << "\n Impossibile Aprire il File";
       		exit(1);
       	}
       	else
       	{
        	for(uint32_t t = 0; t < slotMax; t++)
        	{
			for(uint32_t j = 0; j < numero_sim; j++) 
			{
				if (j == numero_sim-1)
					fout << interestEliminatiFileVect[j][t] << "\n";
				else
					fout << interestEliminatiFileVect[j][t] << "\t";
			}
        	}
       	}
       	fout.close();
}


