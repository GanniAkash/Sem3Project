#include <LiquidCrystal.h>
#include <stdlib.h>
#include <string.h>
#include <Keypad.h>

const int rs = 12, en = 11, d4 = 10, d5 = 9, d6 = 8, d7 = 7;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
typedef char triLogic;

char inp[32] = "";
  int p__ = 0;

const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {14, 15, 16, 17};
byte colPins[COLS] = {5, 4, 3, 2};

Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

#define LOGIC_FALSE     (0)
#define LOGIC_TRUE      (1)
#define LOGIC_DONT_CARE (2)

typedef enum
{
    STATUS_OKAY = 0,
    STATUS_TOO_FEW_VARIABLES,
    STATUS_TOO_MANY_VARIABLES,
    STATUS_OUT_OF_MEMORY,
    STATUS_NULL_ARGUMENT,
} shrinquemStatus;

typedef struct SumOfProducts
{
    unsigned long numVars;
    unsigned long numTerms;
    unsigned long* terms;
    unsigned long* dontCares;
    char* equation;
} SumOfProducts;

void FinalizeSumOfProducts(
    SumOfProducts* sumOfProducts);

shrinquemStatus ReduceLogic(
    const triLogic truthTable[],
    SumOfProducts* sumOfProducts);

shrinquemStatus GenerateEquationString(
    SumOfProducts* sumOfProducts,
    const char** const varNames);

triLogic EvaluateSumOfProducts(
    const SumOfProducts sumOfProducts,
    const unsigned long input);

void ResetTermCounters();
unsigned long GetNumTermsKept();
unsigned long GetNumTermsRemoved();

#define BITS_PER_BYTE (8)

static const unsigned long MAX_NUM_VARIABLES = sizeof(long) * BITS_PER_BYTE;

static unsigned long numTermsKept = 0;
static unsigned long numTermsRemoved = 0;

static unsigned long EstimateMaxNumOfMinterms(
    const unsigned long numVars,
    const triLogic truthTable[]);

static void RemoveNonprimeImplicants(
    SumOfProducts* sumOfProducts);

void FinalizeSumOfProducts(SumOfProducts* sumOfProducts)
{
    sumOfProducts->numVars = 0;
    sumOfProducts->numTerms = 0;

    if (sumOfProducts->terms)
    {
        free(sumOfProducts->terms);
        sumOfProducts->terms = NULL;
    }

    if (sumOfProducts->dontCares)
    {
        free(sumOfProducts->dontCares);
        sumOfProducts->dontCares = NULL;
    }

    if (sumOfProducts->equation)
    {
        free(sumOfProducts->equation);
        sumOfProducts->equation = NULL;
    }
}

shrinquemStatus ReduceLogic(
    const triLogic truthTable[],
    SumOfProducts* sumOfProducts)
{
    shrinquemStatus status = STATUS_OKAY;
    triLogic* resolved = NULL;

    if (truthTable == NULL)
    {
        status = STATUS_NULL_ARGUMENT;
        goto cleanupAndExit;
    }
    else if (sumOfProducts->numVars < 1)
    {
        status = STATUS_TOO_FEW_VARIABLES;
        goto cleanupAndExit;
    }
    else if (sumOfProducts->numVars > MAX_NUM_VARIABLES)
    {
        status = STATUS_TOO_MANY_VARIABLES;
        goto cleanupAndExit;
    }

    unsigned long sizeTruthtable = 1 << sumOfProducts->numVars;
    unsigned long maxNumOfMinterms = EstimateMaxNumOfMinterms(sumOfProducts->numVars, truthTable);
    sumOfProducts->numTerms = 0;
    sumOfProducts->terms = NULL;
    sumOfProducts->dontCares = NULL;
    resolved = calloc(sizeTruthtable, sizeof(triLogic));
    sumOfProducts->terms = malloc(maxNumOfMinterms * sizeof(long));
    sumOfProducts->dontCares = malloc(maxNumOfMinterms * sizeof(long));

    if (resolved == NULL || sumOfProducts->terms == NULL || sumOfProducts->dontCares == NULL)
    {
        status = STATUS_OUT_OF_MEMORY;
        goto cleanupAndExit;
    }

    for (unsigned long iInput = 0; iInput < sizeTruthtable; iInput++)
    {
        if ((truthTable[iInput] == LOGIC_TRUE) && !resolved[iInput])
        {
            unsigned long iTerm = sumOfProducts->numTerms;
            sumOfProducts->numTerms++;
            sumOfProducts->terms[iTerm] = iInput;
            sumOfProducts->dontCares[iTerm] = 0;

            
            for (unsigned long iBitTest = 0; iBitTest < sumOfProducts->numVars; iBitTest++)
            {
                unsigned long bitMaskTest = 1 << iBitTest;
                sumOfProducts->terms[iTerm] ^= bitMaskTest;
                sumOfProducts->terms[iTerm] &= ~(sumOfProducts->dontCares[iTerm]);

                while (1)
                {
                    if (truthTable[sumOfProducts->terms[iTerm]] == LOGIC_FALSE)
                    {
                        sumOfProducts->terms[iTerm] ^= bitMaskTest;
                        break;
                    }
                    unsigned long iBitDC;
                    for (iBitDC = 0; iBitDC < iBitTest; iBitDC++)
                    {
                        unsigned long bitMaskDC = 1 << iBitDC;
                        if (sumOfProducts->dontCares[iTerm] & bitMaskDC)
                        {
                            if (sumOfProducts->terms[iTerm] & bitMaskDC)
                            {
                                sumOfProducts->terms[iTerm] &= ~bitMaskDC;
                            }
                            else
                            {
                                sumOfProducts->terms[iTerm] |= bitMaskDC;
                                break;
                            }
                        }
                    }

                    if (iBitDC == iBitTest)
                    {
                        sumOfProducts->dontCares[iTerm] |= bitMaskTest;
                        break;
                    }
                }
            }

            sumOfProducts->terms[iTerm] &= ~(sumOfProducts->dontCares[iTerm]);

            while (1)
            {
                resolved[sumOfProducts->terms[iTerm]] = LOGIC_TRUE;

                unsigned long iBitDC;
                for (iBitDC = 0; iBitDC < sumOfProducts->numVars; iBitDC++)
                {
                    unsigned long bitMaskDC = 1 << iBitDC;
                    if (sumOfProducts->dontCares[iTerm] & bitMaskDC)
                    {
                        if (sumOfProducts->terms[iTerm] & bitMaskDC)
                        {
                            sumOfProducts->terms[iTerm] &= ~bitMaskDC;
                        }
                        else
                        {
                            sumOfProducts->terms[iTerm] |= bitMaskDC;
                            break;
                        }
                    }
                }

                if (iBitDC == sumOfProducts->numVars)
                {
                    break;
                }
            }
        }
    }

cleanupAndExit:

    if (resolved)
    {
        free(resolved);
        resolved = NULL;
    }

    if (status == STATUS_OKAY)
    {
        void* p;
        p = realloc(sumOfProducts->terms, sumOfProducts->numTerms * sizeof(long));
        sumOfProducts->terms = (p || sumOfProducts->numTerms == 0) ? p : sumOfProducts->terms;
        p = realloc(sumOfProducts->dontCares, sumOfProducts->numTerms * sizeof(long));
        sumOfProducts->dontCares = (p || sumOfProducts->numTerms == 0) ? p : sumOfProducts->dontCares;

        RemoveNonprimeImplicants(sumOfProducts);
    }

    if (status != STATUS_OKAY)
    {
        if (sumOfProducts->numTerms)
            sumOfProducts->numTerms = 0;

        if (sumOfProducts->terms)
        {
            free(sumOfProducts->terms);
            sumOfProducts->terms = NULL;
        }

        if (sumOfProducts->dontCares)
        {
            free(sumOfProducts->dontCares);
            sumOfProducts->dontCares = NULL;
        }
    }

    return status;
}


shrinquemStatus GenerateEquationString(
    SumOfProducts* sumOfProducts,
    const char** const varNames)
{
    unsigned long iVar;
    unsigned long iTerm;
    unsigned long bitMask;
    char** varNamesAuto = NULL;
    const char** varNamesToUse = NULL;
    size_t* varNameSizes = NULL;

    sumOfProducts->equation = NULL;

    if (sumOfProducts->numTerms <= 0)
    {
        sumOfProducts->equation = (char*)malloc(2 * sizeof(char));
        if (sumOfProducts->equation == NULL)
            return STATUS_OUT_OF_MEMORY;

        sumOfProducts->equation[0] = '0';
        sumOfProducts->equation[1] = 0;
        return STATUS_OKAY;
    }

    if (sumOfProducts->numTerms == 1)
    {
        for (iVar = 0; iVar < sumOfProducts->numVars; iVar++)
        {
            bitMask = 1 << iVar;
            if ((sumOfProducts->dontCares[0] & bitMask) == 0)
            {
                break;
            }
        }

        if (iVar == sumOfProducts->numVars)
        {
            sumOfProducts->equation = (char*)malloc(2 * sizeof(char));
            if (sumOfProducts->equation == NULL)
                return STATUS_OUT_OF_MEMORY;
            sumOfProducts->equation[0] = '1';
            sumOfProducts->equation[1] = 0;
            return STATUS_OKAY;
        }
    }

    if (varNames != NULL)
    {
        varNamesToUse = varNames;
    }
    else
    {
        varNamesAuto = (char**)malloc(sumOfProducts->numVars * sizeof(char*));
        varNamesToUse = varNamesAuto;
        if (varNamesAuto == NULL)
            return STATUS_OUT_OF_MEMORY;

        for (iVar = 0; iVar < sumOfProducts->numVars; iVar++)
        {
            varNamesAuto[iVar] = NULL;
        }

        for (iVar = 0; iVar < sumOfProducts->numVars; iVar++)
        {
            if (varNamesAuto[iVar] == NULL)
            {
                varNamesAuto[iVar] = (char*)malloc(2 * sizeof(char));
                if (varNamesAuto[iVar] == 0)
                    return STATUS_OUT_OF_MEMORY;
                (varNamesAuto[iVar])[0] = 'A' + (char)iVar;
                (varNamesAuto[iVar])[1] = 0;
            }
        }
    }

    varNameSizes = (size_t*)malloc(sizeof(long*) * sumOfProducts->numVars);
    if (varNameSizes == NULL)
        return STATUS_OUT_OF_MEMORY;

    for (iVar = 0; iVar < sumOfProducts->numVars; iVar++)
    {
        varNameSizes[iVar] = strlen(varNamesToUse[iVar]);
    }

    size_t outputSize = 0;

    for (iTerm = 0; iTerm < sumOfProducts->numTerms; iTerm++)
    {
        for (iVar = 0; iVar < sumOfProducts->numVars; iVar++)
        {
            bitMask = 1 << (sumOfProducts->numVars - iVar - 1);
            if ((sumOfProducts->dontCares[iTerm] & bitMask) == 0)
            {
                outputSize += varNameSizes[iVar];
                if ((sumOfProducts->terms[iTerm] & bitMask) == 0)
                {
                    outputSize++;
                }
            }
        }

        if (iTerm < (sumOfProducts->numTerms - 1))
        {
            outputSize += 3;
        }
        else
        {
            outputSize++;
        }
    }

    sumOfProducts->equation = (char*)malloc(outputSize * sizeof(char*));
    if (sumOfProducts->equation == NULL)
        return STATUS_OUT_OF_MEMORY;

    unsigned long iEquPos = 0;
    for (iTerm = 0; iTerm < sumOfProducts->numTerms; iTerm++)
    {
        for (iVar = 0; iVar < sumOfProducts->numVars; iVar++)
        {
            bitMask = 1 << (sumOfProducts->numVars - iVar - 1);
            if ((sumOfProducts->dontCares[iTerm] & bitMask) == 0)
            {
                
                for (unsigned long iCharPos = 0; iCharPos < varNameSizes[iVar]; iCharPos++)
                {
                    sumOfProducts->equation[iEquPos++] = (varNamesToUse[iVar])[iCharPos];
                }

                if ((sumOfProducts->terms[iTerm] & bitMask) == 0)
                {
                    sumOfProducts->equation[iEquPos++] = '\'';
                }
            }
        }

        if (iTerm < (sumOfProducts->numTerms - 1))
        {
            sumOfProducts->equation[iEquPos++] = ' ';
            sumOfProducts->equation[iEquPos++] = '+';
            sumOfProducts->equation[iEquPos++] = ' ';
        }
        else
        {
            sumOfProducts->equation[iEquPos++] = 0;
        }
    }

    if (varNamesAuto != NULL)
    {
        for (iVar = 0; iVar < sumOfProducts->numVars; iVar++)
        {
            if (varNamesAuto[iVar] != NULL)
            {
                free(varNamesAuto[iVar]);
                varNamesAuto[iVar] = NULL;
            }
        }

        if (varNamesAuto)
        {
            free(varNamesAuto);
            varNamesAuto = NULL;
        }
    }

    if (varNameSizes)
    {
        free(varNameSizes);
        varNameSizes = NULL;
    }

    return STATUS_OKAY;
}

triLogic EvaluateSumOfProducts(
    const SumOfProducts sumOfProducts,
    const unsigned long input)
{
    const unsigned long inputMask = (1 << sumOfProducts.numVars) - 1;
    unsigned long constrainedInput = input & inputMask;

    for (unsigned long iTerm = 0; iTerm < sumOfProducts.numTerms; iTerm++)
    {
        if ((constrainedInput | sumOfProducts.dontCares[iTerm]) == (sumOfProducts.terms[iTerm] | sumOfProducts.dontCares[iTerm]))
            return LOGIC_TRUE;
    }
    return LOGIC_FALSE;
}

void ResetTermCounters()
{
    numTermsKept = 0;
    numTermsRemoved = 0;
}

unsigned long GetNumTermsKept()
{
    return numTermsKept;
}

unsigned long GetNumTermsRemoved()
{
    return numTermsRemoved;
}


static unsigned long EstimateMaxNumOfMinterms(
    const unsigned long numVars,
    const triLogic truthTable[])
{
    unsigned long sizeTruthtable = 1 << numVars;
    unsigned long maximumPossibleNumOfMinterms = sizeTruthtable / 2;

    unsigned long numTrueMinterms = 0;
    for (unsigned long iInput = 0; iInput < sizeTruthtable; iInput++)
    {
        if (truthTable[iInput] == LOGIC_TRUE)
        {
            numTrueMinterms++;
        }
    }

    return min(maximumPossibleNumOfMinterms, numTrueMinterms);
}

static void RemoveNonprimeImplicants(
    SumOfProducts* sumOfProducts)
{
    unsigned long* refCntTable;
    unsigned long sizeTruthtable;
    unsigned long numOldTerms;
    unsigned long iOldTerm;
    unsigned long iNewTerm;
    unsigned long bitMaskDC;
    char isPrime;

    numOldTerms = sumOfProducts->numTerms;

    sizeTruthtable = 1 << sumOfProducts->numVars;

    refCntTable = (long*)calloc(sizeTruthtable, sizeof(long));

    
    for (iOldTerm = 0; iOldTerm < numOldTerms; iOldTerm++)
    {
        
        sumOfProducts->terms[iOldTerm] &= ~(sumOfProducts->dontCares[iOldTerm]);

        while (1)
        {
            refCntTable[sumOfProducts->terms[iOldTerm]]++;

            
            unsigned long iBitDC;
            for (iBitDC = 0; iBitDC < sumOfProducts->numVars; iBitDC++)
            {
                bitMaskDC = 1 << iBitDC;
                if (sumOfProducts->dontCares[iOldTerm] & bitMaskDC)
                {
                    if (sumOfProducts->terms[iOldTerm] & bitMaskDC)
                    {
                        sumOfProducts->terms[iOldTerm] &= ~bitMaskDC;
                    }
                    else
                    {
                        sumOfProducts->terms[iOldTerm] |= bitMaskDC;
                        break;
                    }
                }
            }
            if (iBitDC == sumOfProducts->numVars)
            {
                break;
            }
        }
    }

    
    for (iNewTerm = iOldTerm = 0; iOldTerm < numOldTerms; iOldTerm++)
    {
        isPrime = 0;
        sumOfProducts->terms[iOldTerm] &= ~(sumOfProducts->dontCares[iOldTerm]);

        while (1)
        {
            if (refCntTable[sumOfProducts->terms[iOldTerm]] == 1)
            {
                isPrime = 1;
                break;
            }

            unsigned long iBitDC;
            for (iBitDC = 0; iBitDC < sumOfProducts->numVars; iBitDC++)
            {
                bitMaskDC = 1 << iBitDC;
                if (sumOfProducts->dontCares[iOldTerm] & bitMaskDC)
                {
                    if (sumOfProducts->terms[iOldTerm] & bitMaskDC)
                    {
                        sumOfProducts->terms[iOldTerm] &= ~bitMaskDC;
                    }
                    else
                    {
                        sumOfProducts->terms[iOldTerm] |= bitMaskDC;
                        break;
                    }
                }
            }
            if (iBitDC == sumOfProducts->numVars)
            {
                break;
            }
        }

        if (isPrime)
        {
            
            if (iOldTerm != iNewTerm)
            {
                sumOfProducts->terms[iNewTerm] = sumOfProducts->terms[iOldTerm];
                sumOfProducts->dontCares[iNewTerm] = sumOfProducts->dontCares[iOldTerm];
            }
            iNewTerm++;
            numTermsKept++;
        }
        else
        {

            sumOfProducts->numTerms--;
            numTermsRemoved++;

            
            sumOfProducts->terms[iOldTerm] &= ~(sumOfProducts->dontCares[iOldTerm]);
            while (1)
            {
                refCntTable[sumOfProducts->terms[iOldTerm]]--;

                
                unsigned long iBitDC;
                for (iBitDC = 0; iBitDC < sumOfProducts->numVars; iBitDC++)
                {
                    bitMaskDC = 1 << iBitDC;
                    if (sumOfProducts->dontCares[iOldTerm] & bitMaskDC)
                    {
                        if (sumOfProducts->terms[iOldTerm] & bitMaskDC)
                        {
                            sumOfProducts->terms[iOldTerm] &= ~bitMaskDC;
                        }
                        else
                        {
                            sumOfProducts->terms[iOldTerm] |= bitMaskDC;
                            break;
                        }
                    }
                }
                if (iBitDC == sumOfProducts->numVars)
                {
                    break;
                }
            }
        }
    }

    free(refCntTable);

    if (sumOfProducts->numTerms != numOldTerms)
    {
        void* p;
        p = realloc(sumOfProducts->terms, sumOfProducts->numTerms * sizeof(long));
        sumOfProducts->terms = (p || sumOfProducts->numTerms == 0) ? p : sumOfProducts->terms;
        p = realloc(sumOfProducts->dontCares, sumOfProducts->numTerms * sizeof(long));
        sumOfProducts->dontCares = (p || sumOfProducts->numTerms == 0) ? p : sumOfProducts->dontCares;
    }
}

void setup() {
  Serial.begin(9600);
  pinMode(A5, OUTPUT);
  analogWrite(A5, 512);
  lcd.begin(16, 2);
}

void loop() {
  lcd.setCursor(0, 0);
  lcd.print("Enter no. vars:");
  lcd.setCursor(0, 1);
  int numVars;
  int numMins;
  char inp_[32] = "";
  int pt_ = 0;
  char key;
  while(1) {
    key = NULL;
    while(!key) {
      key = keypad.getKey();
    }
    if(strcmp(key, 'A') == 0 || strcmp(key, 'C') == 0 || strcmp(key, '*') == 0 || strcmp(key, '#')==0) continue;
    else if(strcmp(key, 'B') == 0) {
      if(pt_ != 0) {
        inp_[pt_-1] = NULL;
        pt_=pt_-1;
      }
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      lcd.print(inp_);
    }
    else if(strcmp(key, 'D') != 0) {
      inp_[pt_] = key;
      pt_ += 1;
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      lcd.print(inp_);
    }
    else {
      lcd.clear();
      numVars = atoi(inp_);
      strcpy(inp_, "");
      pt_ = 0;
      break;
    }
  }

  lcd.setCursor(0, 0);
  lcd.print("Enter no. mins: ");
  strcpy(inp_, "");
  pt_ = 0;
  while(1) {
    key = NULL;
    while(!key) {
      key = keypad.getKey();
    }
    if(strcmp(key, 'A') == 0 || strcmp(key, 'C') == 0 || strcmp(key, '*') == 0 || strcmp(key, '#')==0) continue;
    else if(strcmp(key, 'B') == 0) {
      if(pt_ != 0) {
        inp_[pt_-1] = NULL;
        pt_=pt_-1;
      }
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      lcd.print(inp_);
    }
    else if(strcmp(key, 'D') != 0) {
      inp_[pt_] = key;
      pt_ += 1;
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      lcd.print(inp_);
    }
    else {
      numMins = atoi(inp_);
      strcpy(inp_, "");
      pt_ = 0;
      if(numMins > (int)pow(2, numVars)+1) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(String("Enter <= ") + String(pow(2, numVars)));
        lcd.setCursor(0, 1);
      }
      break;
    }
  }

  Serial.println(pow(2, numVars));
  Serial.println( (int) pow(2, numVars));

  triLogic truthTable[(int) pow(2, numVars) + 1];

  for(int _=0;_<=(int)pow(2, numVars);_++) truthTable[_] = 0;

    for(int i=0;i<pow(2, numVars); i++) {
    Serial.print((int) truthTable[i]);
    Serial.print(" ");
  }

  Serial.println(" ");

  for(int _ = 0; _<numMins; _++) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(String("Enter min ")+String(_)+String(":"));
    strcpy(inp_, "");
    pt_ = 0;
    while(1) {
      key = NULL;
      while(!key) {
        key = keypad.getKey();
      }
      if(strcmp(key, 'A') == 0 || strcmp(key, 'C') == 0 || strcmp(key, '*') == 0 || strcmp(key, '#')==0) continue;
      else if(strcmp(key, 'B') == 0) {
        if(pt_ != 0) {
          inp_[pt_-1] = NULL;
          pt_=pt_-1;
        }
        lcd.setCursor(0, 1);
        lcd.print("                ");
        lcd.setCursor(0, 1);
        lcd.print(inp_);
      }
      else if(strcmp(key, 'D') != 0) {
        inp_[pt_] = key;
        pt_ += 1;
        lcd.setCursor(0, 1);
        lcd.print("                ");
        lcd.setCursor(0, 1);
        lcd.print(inp_);
      }
      else {
        int temp = atoi(inp_);
        if(temp > (int)pow(2, numVars)) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print(String("Enter < ") + String(pow(2, numVars)));
          lcd.setCursor(0, 1);
        }

        truthTable[temp] = 1;
        break;
      }
    }
  }

  SumOfProducts sumOfProducts = { numVars };
  ReduceLogic(truthTable, &sumOfProducts);
  GenerateEquationString(&sumOfProducts, NULL);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(sumOfProducts.equation);

  key = NULL;
  while(1) {
    while(!key) {
      key = keypad.getKey();
    }
    if(strcmp(key, 'D') == 0) {
      break;
    }
    key = NULL;
  }
}