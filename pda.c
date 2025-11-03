#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct State {
    struct Transition *transitions;
    size_t numTransitions;
    bool accept;
};

struct Transition {
    struct State *next;
    bool wildcard;
    char *transitionChars;
};

struct NFA {
    struct State *states;
    size_t numStates;
};

struct State newState(bool accepting) {
    struct State st = {
        .accept = accepting,
        .transitions = NULL,
        .numTransitions = 0
    };
    return st;
}

void addTransition(struct State *st, struct Transition tr) {
    if (st->numTransitions > 0) {
        st->transitions = realloc(st->transitions, (++st->numTransitions)*sizeof(struct Transition));
    } else {
        st->transitions = malloc((++st->numTransitions)*sizeof(struct Transition));
    }
    st->transitions[st->numTransitions-1] = tr;
}

struct Transition newTransition(const char *tchars, struct State *next) {
    struct Transition tr = {
        .next = next,
        .wildcard = false,
        .transitionChars = malloc(sizeof(char)*strlen(tchars)+1)
    };
    strcpy(tr.transitionChars, tchars);
    return tr;
}

struct Transition newWildcardTransition(struct State *next) {
    struct Transition tr = {
        .next = next,
        .wildcard = true,
        .transitionChars = malloc(1)
    };
    tr.transitionChars[0] = '\0';
    return tr;
}

// --------------------------------------------------------------------------- // 
// --------------------------------------------------------------------------- // 

static bool addStateIfNew(struct State ***stateSet, size_t *numStates, size_t *capacity, struct State *state) {
    // Check if already in set
    for (size_t i = 0; i < *numStates; i++) {
        if ((*stateSet)[i] == state) {
            return false;
        }
    }

    // Expand capacity if needed
    if (*numStates >= *capacity) {
        *capacity = (*capacity == 0) ? 8 : (*capacity * 2);
        *stateSet = realloc(*stateSet, *capacity * sizeof(struct State *));
    }

    (*stateSet)[(*numStates)++] = state;
    return true;
}

static void epsilonClosure(struct State ***stateSet, size_t *numStates, size_t *capacity, size_t **startPositions) {
    for (size_t i = 0; i < *numStates; i++) {
        struct State *current = (*stateSet)[i];

        for (size_t tr = 0; tr < current->numTransitions; tr++) {
            if (strlen(current->transitions[tr].transitionChars) == 0 && !current->transitions[tr].wildcard) {
                // Check if already in set
                bool found = false;
                for (size_t j = 0; j < *numStates; j++) {
                    if ((*stateSet)[j] == current->transitions[tr].next) {
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    if (*numStates >= *capacity) {
                        *capacity = (*capacity == 0) ? 8 : (*capacity * 2);
                        *stateSet = realloc(*stateSet, *capacity * sizeof(struct State *));
                        *startPositions = realloc(*startPositions, *capacity * sizeof(size_t));
                    }
                    (*stateSet)[*numStates] = current->transitions[tr].next;
                    (*startPositions)[*numStates] = (*startPositions)[i];  // Inherit start position
                    (*numStates)++;
                }
            }
        }
    }
}

int runNFA(struct State *start, char *input, bool search, bool greedy, int *matchLength) {
    size_t capacity = 8;
    size_t numCurrentStates = 1;
    struct State **currentStates = malloc(capacity * sizeof(struct State *));
    size_t *startPositions = malloc(capacity * sizeof(size_t));
    currentStates[0] = start;
    startPositions[0] = 0;
    
    // Track the best (longest) match found so far
    int bestMatchStart = -1;
    int bestMatchLength = 0;
    int firstMatchStart = -1;  // For search mode - lock onto first match position
    
    // Compute initial epsilon closure
    epsilonClosure(&currentStates, &numCurrentStates, &capacity, &startPositions);
    
    // Process each character
    size_t pos = 0;
    for (char *c = input; *c; c++, pos++) {
        size_t nextCapacity = 8;
        size_t numNextStates = 0;
        struct State **nextStates = malloc(nextCapacity * sizeof(struct State *));
        size_t *nextStartPositions = malloc(nextCapacity * sizeof(size_t));
        
        // For each current state, find transitions on character *c
        for (size_t i = 0; i < numCurrentStates; i++) {
            struct State *current = currentStates[i];
            
            for (size_t tr = 0; tr < current->numTransitions; tr++) {
                bool matches;
                if (!(matches = current->transitions[tr].wildcard)) {
                    for (char *tc = current->transitions[tr].transitionChars; *tc; tc++) {
                        if (*tc == *c) {
                            matches = true;
                            break;
                        }
                    }
                }
                
                if (matches) {
                    bool found = false;
                    for (size_t j = 0; j < numNextStates; j++) {
                        if (nextStates[j] == current->transitions[tr].next) {
                            found = true;
                            break;
                        }
                    }
                    
                    if (!found) {
                        if (numNextStates >= nextCapacity) {
                            nextCapacity *= 2;
                            nextStates = realloc(nextStates, nextCapacity * sizeof(struct State *));
                            nextStartPositions = realloc(nextStartPositions, nextCapacity * sizeof(size_t));
                        }
                        nextStates[numNextStates] = current->transitions[tr].next;
                        nextStartPositions[numNextStates] = startPositions[i];
                        numNextStates++;
                    }
                }
            }
        }
        
        // In search mode, keep the start state active
        // If greedy, only keep active until we find first match
        if (search && (!greedy || firstMatchStart == -1)) {
            bool found = false;
            for (size_t j = 0; j < numNextStates; j++) {
                if (nextStates[j] == start) {
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                if (numNextStates >= nextCapacity) {
                    nextCapacity *= 2;
                    nextStates = realloc(nextStates, nextCapacity * sizeof(struct State *));
                    nextStartPositions = realloc(nextStartPositions, nextCapacity * sizeof(size_t));
                }
                nextStates[numNextStates] = start;
                nextStartPositions[numNextStates] = pos + 1;
                numNextStates++;
            }
        }
        
        free(currentStates);
        free(startPositions);

        // Compute epsilon closure of next states
        epsilonClosure(&nextStates, &numNextStates, &nextCapacity, &nextStartPositions);

        currentStates = nextStates;
        startPositions = nextStartPositions;
        numCurrentStates = numNextStates;
        capacity = nextCapacity;
        
        if (numCurrentStates == 0) {
            free(currentStates);
            free(startPositions);
            if (bestMatchStart != -1) {
                if (matchLength) *matchLength = bestMatchLength;
                return bestMatchStart;
            }
            if (matchLength) *matchLength = 0;
            return -1;
        }
        
        // Check for accepting states AFTER consuming the character
        for (size_t i = 0; i < numCurrentStates; i++) {
            if (currentStates[i]->accept) {
                int matchStart = (int)startPositions[i];
                int matchLen = (pos + 1) - startPositions[i];
                
                if (greedy) {
                    // Greedy mode: continue to find longest match
                    if (search) {
                        // In search mode, lock onto the first match position we find
                        if (firstMatchStart == -1) {
                            firstMatchStart = matchStart;
                            bestMatchStart = matchStart;
                            bestMatchLength = matchLen;
                        } else if (matchStart == firstMatchStart) {
                            // Same start position, update if longer
                            if (matchLen > bestMatchLength) {
                                bestMatchLength = matchLen;
                            }
                        }
                    } else {
                        // Not in search mode, just track the longest match
                        if (matchLen > bestMatchLength || bestMatchStart == -1) {
                            bestMatchStart = matchStart;
                            bestMatchLength = matchLen;
                        }
                    }
                } else {
                    // Non-greedy mode: return first match immediately
                    if (matchLength) *matchLength = matchLen;
                    free(currentStates);
                    free(startPositions);
                    return matchStart;
                }
            }
        }
    }
    
    // Already checked all final states in the loop
    free(currentStates);
    free(startPositions);
    
    if (bestMatchStart != -1) {
        if (matchLength) *matchLength = bestMatchLength;
        return bestMatchStart;
    }
    
    if (matchLength) *matchLength = 0;
    return -1;
}

// --------------------------------------------------------------------------- // 
// --------------------------------------------------------------------------- // 

/*
    Supported syntax:
        .: wildcard (any single character)
        ?: 0 or 1 of the previous character
        *: 0 or more of the previous character
        +: 1 or more of the previous character
        () Groups
        \: Escape next character (treat as literal)
*/
struct NFA constructNFA(char *pattern) {
    size_t numStates = 1;
    size_t maxStates = 16;
    struct State *states = malloc(sizeof(struct State)*maxStates);
    states[0] = newState(false);
    
    size_t *groupStack = malloc(sizeof(size_t) * 32);
    size_t stackTop = 0;
    bool lastWasGroup = false;
    bool lastCharWasOperator = false;
    
    for (char *c = pattern; *c; c++) {
        switch (*c) {
            case '.': {
                // Wildcard transition (accept any one character)
                if (maxStates == numStates) {
                    maxStates += 16;
                    states = realloc(states, sizeof(struct State)*maxStates);
                }
                states[numStates] = newState(false);
                
                struct Transition wildTr = newWildcardTransition(&states[numStates]);
                addTransition(&states[numStates-1], wildTr);
                
                numStates++;
                lastWasGroup = false;
                lastCharWasOperator = false;
                break;
            }
            case '(': {
                // Push current state onto stack so we can encapsulate the entire group in a transition if needed
                groupStack[stackTop++] = numStates - 1;
                lastWasGroup = false;
                lastCharWasOperator = false;
                break;
            }
            case ')': {
                // Mark that a group was just closed
                lastWasGroup = true;
                lastCharWasOperator = false;
                break;
            }
            case '?': {
                assert(numStates > 1);
                assert(!lastCharWasOperator);
                lastCharWasOperator = true;
                
                if (lastWasGroup) {
                    // Make the entire group optional
                    size_t groupStart = groupStack[--stackTop];
                    addTransition(&states[groupStart], newTransition("", &states[numStates-1]));
                } else {
                    // Make last character optional
                    addTransition(&states[numStates-2], newTransition("", &states[numStates-1]));
                }
                lastWasGroup = false;
                break;
            }
            case '*': {
                assert(numStates > 1);
                assert(!lastCharWasOperator);
                lastCharWasOperator = true;
                
                if (lastWasGroup) {
                    // Make the entire group repeat zero or more times
                    size_t groupStart = groupStack[--stackTop];
                    size_t groupEnd = numStates - 1;
                    
                    // Loop back: group end -> state right after group start
                    for (size_t i = 0; i < states[groupStart].numTransitions; i++) {
                        struct State *targetState = states[groupStart].transitions[i].next;
                        struct Transition loopTr;
                        if (states[groupStart].transitions[i].wildcard) {
                            loopTr = newWildcardTransition(targetState);
                        } else {
                            loopTr = newTransition(states[groupStart].transitions[i].transitionChars, targetState);
                        }
                        addTransition(&states[groupEnd], loopTr);
                    }
                    // Add epsilon to skip the group entirely
                    addTransition(&states[groupStart], newTransition("", &states[groupEnd]));
                } else {
                    // Make last character repeat zero or more times
                    for (size_t i = 0; i < states[numStates-2].numTransitions; i++) {
                        if (states[numStates-2].transitions[i].next == &states[numStates-1]) {
                            states[numStates-2].transitions[i].next = &states[numStates-2];
                            break;
                        }
                    }
                    numStates--;
                }
                lastWasGroup = false;
                break;
            }
            case '+': {
                assert(numStates > 1);
                assert(!lastCharWasOperator);
                lastCharWasOperator = true;
                
                if (lastWasGroup) {
                    // Make the entire group repeat one or more times
                    size_t groupStart = groupStack[--stackTop];
                    size_t groupEnd = numStates - 1;
                    
                    // Loop back: group end -> state right after group start
                    for (size_t i = 0; i < states[groupStart].numTransitions; i++) {
                        struct State *targetState = states[groupStart].transitions[i].next;
                        struct Transition loopTr;
                        if (states[groupStart].transitions[i].wildcard) {
                            loopTr = newWildcardTransition(targetState);
                        } else {
                            loopTr = newTransition(states[groupStart].transitions[i].transitionChars, targetState);
                        }
                        addTransition(&states[groupEnd], loopTr);
                    }
                } else {
                    // Make last character repeat one or more times
                    // Check if the transition TO the current state is a wildcard
                    bool lastWasWildcard = false;
                    if (numStates >= 2) {
                        for (size_t i = 0; i < states[numStates-2].numTransitions; i++) {
                            if (states[numStates-2].transitions[i].next == &states[numStates-1] &&
                                states[numStates-2].transitions[i].wildcard) {
                                lastWasWildcard = true;
                                break;
                            }
                        }
                    }
                    
                    if (lastWasWildcard) {
                        addTransition(&states[numStates-1], newWildcardTransition(&states[numStates-1]));
                    } else {
                        char pch[2] = {*(c-1), '\0'};
                        addTransition(&states[numStates-1], newTransition(pch, &states[numStates-1]));
                    }
                }
                lastWasGroup = false;
                break;
            }
            case '\\': {
                c++;
                if (!*c) {
                    // Backslash at end of pattern - treat as literal backslash
                    if (maxStates == numStates) {
                        maxStates += 16;
                        states = realloc(states, sizeof(struct State)*maxStates);
                    }
                    states[numStates] = newState(false);
                    char ch[2] = {'\\', '\0'};
                    addTransition(&states[numStates-1], newTransition(ch, &states[numStates]));
                    numStates++;
                    lastWasGroup = false;
                    lastCharWasOperator = false;
                    break;
                }
                // fallthrough to literal character case (for operator characters)
                __attribute__((fallthrough));
            }
            default: {
                // Create transition with this character to new state
                if (maxStates == numStates) {
                    maxStates += 16;
                    states = realloc(states, sizeof(struct State)*maxStates);
                }
                states[numStates] = newState(false);
                char ch[2] = {*c, '\0'};
                addTransition(&states[numStates-1], newTransition(ch, &states[numStates]));
                numStates++;
                lastWasGroup = false;
                lastCharWasOperator = false;
                break;
            }
        }
    }
    
    free(groupStack);
    states = realloc(states, sizeof(struct State)*numStates);
    states[numStates-1].accept = true;
    return (struct NFA) {
        .numStates = numStates,
        .states = states
    };
}

// --------------------------------------------------------------------------- // 
// --------------------------------------------------------------------------- // 

int main(int argc, char *argv[]) {
    bool greedy = false;
    char *pattern = NULL;
    char *filename = NULL;
    
    // Parse command line arguments
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "Usage: %s [-g] <pattern> <filename>\n", argv[0]);
        fprintf(stderr, "  -g: Enable greedy matching (find longest match)\n");
        return 1;
    }
    
    int argIdx = 1;
    
    // Check for -g flag
    if (strcmp(argv[argIdx], "-g") == 0) {
        greedy = true;
        argIdx++;
    }
    
    if (argc - argIdx != 2) {
        fprintf(stderr, "Usage: %s [-g] <pattern> <filename>\n", argv[0]);
        fprintf(stderr, "  -g: Enable greedy matching (find longest match)\n");
        return 1;
    }
    
    pattern = argv[argIdx];
    filename = argv[argIdx + 1];
    
    // Open and read file
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Could not open file '%s'\n", filename);
        return 1;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Read entire file into string
    char *contents = malloc(fileSize + 1);
    if (!contents) {
        fprintf(stderr, "Error: Could not allocate memory\n");
        fclose(file);
        return 1;
    }
    
    size_t bytesRead = fread(contents, 1, fileSize, file);
    contents[bytesRead] = '\0';
    fclose(file);

    if (bytesRead != (size_t)fileSize) {
        fprintf(stderr, "Warning: Expected to read %ld bytes, but read %zu bytes\n", fileSize, bytesRead);
    }
    
    // Construct NFA from pattern
    struct NFA nfa = constructNFA(pattern);
    
    printf("Searching for pattern \"%s\" in file \"%s\" (%s):\n\n", 
           pattern, filename, greedy ? "greedy" : "non-greedy");
    
    // Find all occurrences
    size_t offset = 0;
    int matchCount = 0;
    
    while (offset < strlen(contents)) {
        int matchLen = 0;
        int result = runNFA(&nfa.states[0], contents + offset, true, greedy, &matchLen);
        
        if (result == -1) {
            break;  // No more matches found
        }
        
        size_t matchIndex = offset + result;
        
        // Skip empty matches - require at least one character
        if (matchLen == 0) {
            offset++;
            continue;
        }
        
        // Extract and print the matched string
        char *matchedStr = malloc(matchLen + 1);
        strncpy(matchedStr, contents + matchIndex, matchLen);
        matchedStr[matchLen] = '\0';
        
        printf("Match #%d at index %zu: \"%s\"\n", ++matchCount, matchIndex, matchedStr);
        free(matchedStr);
        
        // Advance past the matched string (non-overlapping matches)
        offset = matchIndex + matchLen;
    }
    
    if (matchCount == 0) {
        printf("No matches found.\n");
    } else {
        printf("\nTotal matches: %d\n", matchCount);
    }
    
    // Cleanup
    for (size_t i = 0; i < nfa.numStates; i++) {
        for (size_t tr = 0; tr < nfa.states[i].numTransitions; tr++) {
            free(nfa.states[i].transitions[tr].transitionChars);
        }
        free(nfa.states[i].transitions);
    }
    free(nfa.states);
    free(contents);
    
    return 0;
}