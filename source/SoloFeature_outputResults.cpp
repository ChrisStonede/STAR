#include "SoloFeature.h"
#include "streamFuns.h"
#include "TimeFunctions.h"
#include "serviceFuns.cpp"
#include "SequenceFuns.h"
#include "ErrorWarning.h"
#include "SoloFeatureTypes.h"

void SoloFeature::outputResults(bool cellFilterYes, string outputPrefixMat)
{    
    //make directories   
    createDirectory(outputPrefixMat,P.runDirPerm, "Solo output directory", P);
    /* old way, does not work when parent directores are needed
    if (mkdir(outputPrefixMat.c_str(),P.runDirPerm)!=0 && errno!=EEXIST) {//create directory
        exitWithError("EXITING because of fatal OUTPUT FILE error: could not create Solo output directory" + outputPrefixMat + 
                      "\nSOLUTION: check the path and permisssions\n",
                       std::cerr, P.inOut->logMain, EXIT_CODE_PARAMETER, P);
    };
    */
    
    /////////////////////////////////////////////////////////////
    //write features.tsv
    switch (featureType) {
        case SoloFeatureTypes::Gene :
        case SoloFeatureTypes::GeneFull :
        case SoloFeatureTypes::GeneFull_Ex50pAS :
        case SoloFeatureTypes::GeneFull_ExonOverIntron :
        case SoloFeatureTypes::Velocyto :
        case SoloFeatureTypes::VelocytoSimple :
        {
            ofstream &geneStr=ofstrOpen(outputPrefixMat+pSolo.outFileNames[1],ERROR_OUT, P);
            for (uint32 ii=0; ii<Trans.nGe; ii++) {
                geneStr << Trans.geID[ii] <<"\t"<< (Trans.geName[ii].empty() ? Trans.geID[ii] : Trans.geName[ii]);
                if (pSolo.outFormat.featuresGeneField3!="-") {
                    geneStr <<'\t'<< pSolo.outFormat.featuresGeneField3;
                };
                geneStr << '\n';
            };
            geneStr.close();
            break;
        };
        case SoloFeatureTypes::SJ :
            remove((outputPrefixMat+pSolo.outFileNames[1]).c_str()); //remove symlink before creating it
            if ( symlink("../../../SJ.out.tab", (outputPrefixMat+pSolo.outFileNames[1]).c_str()) != 0 )
                 exitWithError("EXITING because of fatal OUTPUT FILE error: could not sym-link ../../../SJ.out.tab into" + outputPrefixMat+pSolo.outFileNames[1] +
                      "\nSOLUTION: check the path and permisssions\n",
                       std::cerr, P.inOut->logMain, EXIT_CODE_PARAMETER, P);
    };

    ////////////////////////////////////////////////////////////////////////////
    //write barcodes.tsv
    ofstream &cbStr=ofstrOpen(outputPrefixMat+pSolo.outFileNames[2],ERROR_OUT, P);
    uint64 nCellGeneEntries=0;//total number of non-zero cell/gene combinations (entries in the output matrix)
    if (cellFilterYes) {//filtered cells
        for (uint32 icb=0; icb<nCB; icb++) {
            if (filteredCells.filtVecBool[icb]) {
                cbStr << pSolo.cbWLstr[indCB[icb]] <<'\n';
                nCellGeneEntries += nGenePerCB[icb];
            };
        };
    } else {//unfiltered cells
        for (uint64 ii=0; ii<pSolo.cbWLsize; ii++) {
             cbStr << pSolo.cbWLstr[ii] <<'\n';
        };
        for (uint32 icb=0; icb<nCB; icb++) {
            nCellGeneEntries += nGenePerCB[icb];
        };
    };
    cbStr.flush();   

    /////////////////////////////////////////////////////////////
    //output counting matrix
    
    for (uint32 iCol=1; iCol<countMatStride; iCol++) {

        string matrixFileName=outputPrefixMat;
        if (featureType == SoloFeatureTypes::Velocyto) {
            const array<string,3> velonames = {"spliced.mtx", "unspliced.mtx", "ambiguous.mtx"};
            matrixFileName += velonames[iCol-1];
            
        } else if (iCol>1 && cellFilterYes) {
            break; //if cellFilterYes, only output first iCol, since filtering is only done for it       
            
        } else if (pSolo.umiDedup.types.size()>1) {
            matrixFileName += "umiDedup-" + pSolo.umiDedup.typeNames[pSolo.umiDedup.types[iCol-1]] + ".mtx";
            
        } else {
            matrixFileName += pSolo.outFileNames[3];
        };
        ofstream &countMatrixStream=ofstrOpen(matrixFileName,ERROR_OUT, P);
        
        //header
        countMatrixStream <<"%%MatrixMarket matrix coordinate integer general\n";
        countMatrixStream <<"%\n";
        countMatrixStream << featuresNumber <<' '<< (cellFilterYes ? filteredCells.nCells : pSolo.cbWLsize) <<' '<< nCellGeneEntries << '\n';
        
        uint32  cbInd1=0;
        for (uint32 icb=0; icb<nCB; icb++) {
            if (cellFilterYes) {
                if (filteredCells.filtVecBool[icb]) {
                    ++cbInd1;
                } else {
                    continue;
                };
            } else {
                cbInd1=indCB[icb]+1;
            };
            for (uint32 ig=0; ig<nGenePerCB[icb]; ig++) {
                
                uint32 indG1=countCellGeneUMIindex[icb]+ig*countMatStride;
                
                //feature index, CB index, count
                countMatrixStream << countCellGeneUMI[indG1]+1 <<' '<< cbInd1 <<' '<< countCellGeneUMI[indG1+iCol] << '\n';
            };
        };

        countMatrixStream.close();
    };
    
    //////////////////////////////////////////// output unique+multimappers
    if (pSolo.multiMap.yes.multi && !cellFilterYes
        && (featureType == SoloFeatureTypes::Gene || featureType == SoloFeatureTypes::GeneFull
         || featureType == SoloFeatureTypes::GeneFull_ExonOverIntron || featureType == SoloFeatureTypes::GeneFull_Ex50pAS) ) {
                          //skipping unique
        for (const auto &iMult: pSolo.multiMap.types) {               
            for (uint32 iDed=0; iDed<pSolo.umiDedup.yes.N; iDed++) {
                string matrixFileName=outputPrefixMat + "UniqueAndMult-" + pSolo.multiMap.typeNames[iMult];
                if (pSolo.umiDedup.types.size()>1) {
                    matrixFileName += "_umiDedup-" + pSolo.umiDedup.typeNames[pSolo.umiDedup.types[iDed]];
                };
                matrixFileName += ".mtx";

                uint32 mIndex = pSolo.multiMap.countInd.I[iMult] + iDed;
                    
                //string matOutString;
                //matOutString.reserve(100000000);
                std::ostringstream matOutStringStream;//(matOutString);
                    
                nCellGeneEntries = 0;
                    
                uint32  cbInd1=0;
                for (uint32 icb=0; icb<nCB; icb++) {
                    cbInd1=indCB[icb]+1;
                    
                    /* just unique
                    for (uint32 igm=countCellGeneUMIindex[icb]; igm<countCellGeneUMIindex[icb+1]; igm+=countMatStride) {
                                               
                        //feature index, CB index, count
                        matOutString += to_string(countCellGeneUMI[igm]+1) +' '+ to_string(cbInd1) +' '+ to_string(countCellGeneUMI[igm+iDed]) + '\n';
                        
                        ++nCellGeneEntries;
                    };
                    */
                    
                    /*//just multiple
                    for (uint32 igm=countMatMult.i[icb]; igm<countMatMult.i[icb+1]; igm+=countMatMult.s) {                      
                        matOutStringStream << int(countMatMult.m[igm])+1 <<' '<< cbInd1 <<' '<< countMatMult.m[igm+mIndex] <<'\n';
                        ++nCellGeneEntries;
                    };
                    */
                    
                    //if (false)
                    {//sum unique+multiple: go over sorted lists. TODO: use a map to combine (check efficiency)
                        auto igm1 = countCellGeneUMIindex[icb];
                        auto igm2 = countMatMult.i[icb];
                        while ( igm1<countCellGeneUMIindex[icb+1] || igm2<countMatMult.i[icb+1] ) {
                            uint32 g1,c1=0,g2;
                            double c2=0;
                            
                            if (igm1<countCellGeneUMIindex[icb+1]) {
                                g1 = countCellGeneUMI[igm1];
                                c1 = countCellGeneUMI[igm1+1+iDed];
                            } else {
                                g1 = (uint32)-1;
                            };
                            
                            if (igm2<countMatMult.i[icb+1]) {
                                g2 = countMatMult.m[igm2];
                                c2 = countMatMult.m[igm2+mIndex];                                
                            } else {
                                g2 = (uint32)-1;
                            };
                            
                            if (g1<g2) {
                                matOutStringStream << g1+1 <<' '<< cbInd1 <<' '<< c1 <<'\n';
                                igm1 += countMatStride;
                            } else if (g1>g2) {
                                matOutStringStream << g2+1 <<' '<< cbInd1 <<' '<< c2 <<'\n';
                                igm2 += countMatMult.s;  
                            } else {
                                matOutStringStream << g1+1 <<' '<< cbInd1 <<' '<< c1+c2 <<'\n';
                                igm1 += countMatStride;
                                igm2 += countMatMult.s;  
                            };
                            
                            ++nCellGeneEntries;
                        };
                    };
                };
                //matOutStringStream.flush();
                
                ofstream &countMatrixStream=ofstrOpen(matrixFileName, ERROR_OUT, P);
                
                //header
                countMatrixStream <<"%%MatrixMarket matrix coordinate real general\n";
                countMatrixStream <<"%\n";
                countMatrixStream << featuresNumber <<' '<< pSolo.cbWLsize <<' '<< nCellGeneEntries << '\n';
                countMatrixStream << matOutStringStream.str();

                countMatrixStream.close();
            };
        };
    };
};
