/* Enzo Faia Guerrieri de Castro Matricula 2410302 Turma 3WA*/
/* Arthur Rodrigues Alves Barbosa Matricula 2310394 Turma 3WA*/
# include <stdio.h>
# include <stdlib.h>
# include "gravacomp.h"

int paddingCalc(int currentPlace) {
    int mult = 4;
    while(currentPlace > mult){
        mult = mult + 4;
    }
return mult - currentPlace;
}

int gravacomp(int nstructs, void* valores, char* campos,FILE* arquivo) {
    char* descriptionPointer = campos;
    int intCounter = 0;
    int strCounter = 0;

    fwrite(&nstructs,1,1,arquivo);

    while(*descriptionPointer){
        /*Parse the descriptor once, to determine how many ints and strings there are.*/
        if (*descriptionPointer == 'i' || *descriptionPointer == 'u') {
            intCounter += 1;
        }

        if (*descriptionPointer == 's'){
            strCounter  += 1;
        }
    descriptionPointer++;
    }

    /*Dynamically allocate an array to store the location of all indexes for str and int*/
    /*RELATIVE TO THE START OF THE STRUCT!*/
    int* intIndexList = (int*)malloc(sizeof(int)*intCounter);
    
    int* strIndexList = (int*)malloc(sizeof(int)*strCounter);

    int* mergedSortedElementsIndexList = (int*)malloc(sizeof(int)*(strCounter+intCounter));

    /*Reset description pointer to the start of the description string*/
    descriptionPointer  = campos;

    int intListIndex = 0;
    int strListIndex = 0;
    int currentPlace = 0;
    int sizeStruct = 0;

    /*Parse the description string once more to find out what indexes the ints are stored in, relative to the start of the struct.*/
    while(*descriptionPointer){
        if (*descriptionPointer == 's') {
            int sizeString = 0;
            /*Saves the location for the string.*/
            strIndexList[strListIndex] = currentPlace;

            strListIndex += 1;

            descriptionPointer++; 

            sizeString += (*descriptionPointer - 48) * 10; 

            descriptionPointer++;

            sizeString += (*descriptionPointer - 48);

            /*Add the size of every string to the size of the struct.*/
            sizeStruct += sizeString;

            currentPlace += sizeString;
        }

        if (*descriptionPointer == 'i' || *descriptionPointer == 'u') {

            /*If there is an int, increment the size of the struct by 4.*/

            sizeStruct += 4;
            /*if there is an int, check if the current place is adequate. if not, add padding such that it is.*/
            if (currentPlace % 4 != 0) {
                /*Add the size of any padding to the struct size.*/
                sizeStruct += paddingCalc(currentPlace);
                currentPlace += paddingCalc(currentPlace);
            }

            /*Save the currentPlace to the intIndexList*/
            intIndexList[intListIndex] = currentPlace;
            intListIndex += 1;
            /*Increment the currentPlace by 4, the ammount of bytes in an int.*/
            currentPlace += 4;
            }
        descriptionPointer++;
        }
    /*Reset descriptionPointer to the start of the string so it can be reused later.*/
    descriptionPointer = campos;

        /*If the struct is not divisible by 4 (its most restrictive type), add padding such that it is to the size.*/
    if (sizeStruct % 4 != 0 && intCounter != 0) {
        sizeStruct = sizeStruct + paddingCalc(sizeStruct);
    }

    
    /*Now we create a merged sorted list with all the indexes.*/
    
    int intIndexListIncrements = 0;
    int strIndexListIncrements = 0;
    int* tempInt = intIndexList;
    int* tempIntStr = strIndexList;
    for (int i = 0; i < (intCounter + strCounter); i++) {
        /*if intIndexList is over, insert all values of strIndexList and vice-versa.*/

        if (intIndexListIncrements >= intCounter){
            mergedSortedElementsIndexList[i] = *tempIntStr;
            strIndexListIncrements += 1;
            if (strIndexListIncrements < strCounter) {
                tempIntStr++;
            }
            continue;
        }

        if (strIndexListIncrements >= strCounter){
            mergedSortedElementsIndexList[i] = *tempInt;
            intIndexListIncrements += 1;
            if (intIndexListIncrements < intCounter){
                tempInt++;
            }
            continue;
        }

        /*if neither are over, proceed with normal merge sort and move the pointer forwards when the value is added and it is possible.*/
        if (*tempInt < *tempIntStr) {
            mergedSortedElementsIndexList[i] = *tempInt;
            intIndexListIncrements++;
            if (intIndexListIncrements < intCounter){
                tempInt++;
            }
        }

        else if (*tempIntStr < *tempInt){
            mergedSortedElementsIndexList[i] = *tempIntStr;
            strIndexListIncrements++;
            if (strIndexListIncrements < strCounter){
                tempIntStr++;
            }
        }
    }
    free(intIndexList);
    free(strIndexList);
    intIndexList = NULL;
    strIndexList = NULL;

    /*Now we have all of the necessary locations where an int will be, we can parse the array of structs and compactly store each member.*/

    int structNumber = 0;

    /*Iterate through all the structs. When we reach an increment counter that is the size of a struct, the struct is over.*/
    /*Then, we reset the increment counter and start counting again for the new struct.*/
    while (structNumber < nstructs){
        unsigned char* pointerForStruct = valores;
        pointerForStruct += sizeStruct*structNumber;
        int indexForDescriptor = 0;

        for(int i = 0; i < (intCounter+strCounter); i++) {
            unsigned char* tempPointerToStartOfStruct = pointerForStruct;

            pointerForStruct = pointerForStruct + mergedSortedElementsIndexList[i];

            int headerByte = 0;

            int numBytes = 0;

            /*Tests if the element is the last one for the struct.*/
            if(i == (intCounter+strCounter)-1) { headerByte = 0x80;}

            

            if(descriptionPointer[indexForDescriptor] == 'i' || descriptionPointer[indexForDescriptor] == 'u') {
                /*Declares a void ptr to the current struct member for conversion.*/
                void* tempVoidPtrForConversion = pointerForStruct;
                unsigned char* bytesForIntWrite[4] = {NULL, NULL, NULL, NULL};

                for(int i = 0; i < 4; i++){
                    bytesForIntWrite[i] = pointerForStruct+i;
                }

                /*Determines the ammount of bytes necessary to store the signed int.*/
                if (descriptionPointer[indexForDescriptor] == 'i'){
                int* tempPointerForSInt = tempVoidPtrForConversion;

                    headerByte = (headerByte | 0x20);

                    if (*tempPointerForSInt >= -128 && *tempPointerForSInt < 128){
                        numBytes = 1;
                    }
                    else if (*tempPointerForSInt >= -32768 && *tempPointerForSInt < 32768){
                        numBytes = 2;
                    }
                    else if (*tempPointerForSInt >= -8388608 && *tempPointerForSInt < 8388608) {
                        numBytes = 3;
                    }

                    else {
                        numBytes = 4;
                    }
                }

                /*Determines the ammount of bytes necessary to store the unsigned int.*/
                else if(descriptionPointer[indexForDescriptor] == 'u') { 
                    unsigned int* tempPointerForUInt = tempVoidPtrForConversion;
                    if (*tempPointerForUInt <= 255) {
                        numBytes = 1;
                    }
                    else if (*tempPointerForUInt <= 65535) {
                        numBytes = 2;
                    }
                    else if (*tempPointerForUInt <= 16777215) {
                        numBytes = 3;
                    }
                    else {
                        numBytes = 4;
                    }
                }
                
            /*Prints the header byte array in inverse order, making it so it is printed BIG ENDIAN in the file.*/
            headerByte = headerByte|numBytes;
            fwrite(&headerByte,1,1,arquivo);
            for(int i = numBytes-1; i > -1; i--){
                fwrite(bytesForIntWrite[i],1,1,arquivo);
                }
            indexForDescriptor += 1;
            }

            else if(descriptionPointer[indexForDescriptor] == 's') {

                int sizeOfStringInStruct = 0;

                unsigned char* pointerForStructForIncrement = pointerForStruct;

                /*strlen that won't throw a warning with an unsigned char.*/
                while(*pointerForStructForIncrement != '\0'){
                    sizeOfStringInStruct  += 1;
                    pointerForStructForIncrement++;
                }
                /*Adding 1 for padding.*/
                sizeOfStringInStruct++;


                unsigned char *tempStr = (unsigned char*)malloc(sizeof(unsigned char) * sizeOfStringInStruct);

                /*strcpy that won't throw a warning for an unsigned char.*/
                unsigned char *tempStrForIncrement = tempStr;
                while(*pointerForStruct != '\0'){

                    *tempStrForIncrement = *pointerForStruct;

                    tempStrForIncrement++;
                    pointerForStruct++;
                }

                headerByte = (headerByte | 0x40);

                headerByte += sizeOfStringInStruct-1;

                fwrite(&headerByte,1,1,arquivo);
                fwrite(tempStr,1,sizeOfStringInStruct-1,arquivo);

                free(tempStr);

                indexForDescriptor+=3;
                
            }
            pointerForStruct = tempPointerToStartOfStruct;
        }
        
        /*At the end of iterating through a struct, go back and iterate the next.*/
        pointerForStruct = valores;
        structNumber += 1;
    }

    free(mergedSortedElementsIndexList);
    mergedSortedElementsIndexList = NULL;
    return 0;
}

void mostracomp(FILE* arquivo) {

    unsigned char byte;  // Variable to hold each byte read from the file
    fread(&byte, sizeof(char), 1, arquivo);  // Read the number of structures from the file
    unsigned int nStructs = byte;  // Store the number of structures

    unsigned int currentStruct = 0;  // Variable to track the current structure being processed
    int lastElementOfStruct;
    // Display the total number of structures
    printf("Estruturas: %d\n", nStructs);

    // Loop through all the structures in the file
    while(currentStruct < nStructs){
        lastElementOfStruct = 0;
        
        while(lastElementOfStruct != 1){
            fread(&byte, sizeof(unsigned char), 1, arquivo);//reading the header byte of the element in struct
            unsigned char headerByte = byte;
            //Now we are going to check the type of this variable, in order to show it in the correct way:
            if((headerByte & 0x40) == 0x40){//string type
                char string[64];
                int sizeOfString = headerByte & 0x3F;//string size.
                int x = 0;
                for(x = 0; x < sizeOfString; x++){
                    fread(&byte, sizeof(char), 1, arquivo);
                    string[x] = byte;
                }
                string[x] = '\0';

                printf("(str) %s\n", string);//Printing the string in specified format
            }


            else if((headerByte & 0x20) == 0x20){//signed int type
                unsigned int integer = 0; // integer that will store the byte conbination in file
                int sizeInteger = headerByte & 0x1F;//selecting the (size of int) bits.

                for(int i = 0; i < sizeInteger; i++){
                    fread(&byte, sizeof(unsigned char), 1, arquivo);//Now we must read the (sizeInteger) bits of this number, in order to build it from scratch
                    integer = (integer << 8) | byte; // Shift the previous bytes and add the new byte
                }

                int signedInteger = (int)integer;

                unsigned int signMask = 1U << (sizeInteger*8 - 1);

                unsigned int restMask = (1U << sizeInteger*8) -1;


                if ((integer & signMask) && sizeInteger <4 ) {/*Checks if the bytes we have are negative by shifting 1 into their MSB and checking for a non negative value.*/

                    signedInteger = signedInteger | ~restMask; /*Masks all lower bits than the ones we need with 1s and then masks the number we have with the opposite of that.*/
                    /*Makes all higher bits 1.*/
                }

                printf("(int) %d (%08x)\n", signedInteger, signedInteger); // Print the signed integer value in both decimal and hexadecimal format

                } 

            else{//unsigned int type
                unsigned int uInt = 0; // unsigned integer that will store the byte combination in file
                int sizeInteger = headerByte & 0x1F;//selecting the (size of int) bits. 

                for(int i = 0; i < sizeInteger; i++){
                    fread(&byte, sizeof(unsigned char), 1, arquivo);//Now we must read the (sizeInteger) bits of this number, in order to build it from scratch
                    uInt = (uInt << 8) | byte; // Shift the previous bytes and add the new byte
                }
                 // Print the signed integer value in both decimal and hexadecimal format
                printf("(uns) %u (%08x)\n", uInt, uInt);
            }


            if ((headerByte & 0x80) == 0x80){//we reached the last element of the struct
                lastElementOfStruct = 1;
            }
        }
        printf("\n");
        currentStruct++;
    }
}



