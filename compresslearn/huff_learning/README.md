TODO:

VERSION_1: BASIC MOFFAT-TURPIN VARIANT          
VERSION 2: ADD TABLE-ACCELERATED DECODE         
VERSION_3: ADD MOFFAT-TURPIN FAST DECODE        
VERSION_4: ADD NELSON VARIANT AND BENCHMARK    

Here's my understanding of Moffat-Turpin (a lot of it courtesy of cbloom):

We have an n-symbol alphabet with n corresponding frequencies in the data.  Given a probability-sorted list of these freqs, we can find the corresponding codelens.  See moffat-katajainen / Polar for example.  And from the codelens and assoc symbols, we can create
the codes:

code lens :    

c : 1    
b : 2    
a : 3    
d : 3    
    
[code=0]    

c : [0]    
    code ++ [1]    
    len diff = (2-1) so code <<= 1 [10]    
b : [10]    
    code ++ [11]    
    len diff = (3-2) so code <<= 1 [110]    
a : [110]    
    code ++ [111]    
    no len diff    
d : [111]    

Note that Moffat-Turpin employ a nearly identical scheme, except that they start the most probable symbol off with a code of all 1 bits and subtract instead of add.  When the length increases, Moffat-Turpin left shift and then subtract, as opposed to
add and then left shift.  

Then we can encode our data by doing the following:
code <- base[length] + (symbol_id - offset[length])       //find the basecode and add i for the ith symbol with that length

We figure out the length by searching for the greatest length s.t. offset[length] <= symbol_id.  Then the code can determined pretty simply. 

The offset table and symbol_ids can be assigned implicitly from the codelens.

And we can decode our data by doing the following:
symbol_id <- offset[length] + (code - base[length])

Which will return (need to double-check) a symbol in "canonical" order, with most probable being 0.
The base and offset tables and symbol_ids are created the same way as in the encoder.

Note that we can figure out the correct length in a stream as follows:
The first decode iter per symbol, we read in the N = min-codelen number of bits.  After that, a bit each time.

For each iter, we check if code > (basecode[length] + num_codes_for_codelen - 1) 
If true, read in more bits, else decode.

I'm not super pleased with this, but we do already have the num_codes_for_codelen when we generate basecodes.

Here's another possibility, from cbloom.  It is as follows:
You can find the node you should be at any time by doing :

index = StartIndex[ codeLen ] + code;

StartIndex[codeLen] contains the index of the first code of that length *minus* the value of the first code bits at that len. You
could compute StartIndex in O(1) , but in practice you should put them in a table; you only need 16 of them, one for each code len.

That is, create StartIndex such that StartIndex[l] = offset[l] - basecode[l]

So the full decoder looks like :
code = 0;
codeLen = 0;

for(;;)
{
    code <<= 1;
    code |= readbit();
    codeLen ++;

    if ( Table[ StartIndex[ codeLen ] + code ].codeLen == codeLen )
        return Table[ StartIndex[ codeLen ] + code ].Symbol
}

So for example, if you read in bits 01, then StartIndex[ 2 ] = 0 - 00 = 0.  Then you index into the table as:
Table[ 0 + 1].codelen = 3, which != 2, so you read another bit.

As another example, if you read in 101, then StartIndex[ 3 ] = 1 - 2 = -1.  Then you index into the table as:
Table[ -1 + 5].codelen = 4, which != 3, so you read another bit.

Finally, if you read in bits 1011, then StartIndex[ 4 ] = 4 - 10 = -6.  Then you index into the table as:
Table[ -6 + 11].codelen = 4, which == 4

So yeah, holy shit.  This makes sense, too.  You're grabbing the differential between the basecode and your current code
and adding that to the index.  This works because of how you build up the codes.

Interestingly, this decode is basically the same as Moffat-Turpin, except that the condition depends on codelen
instead of basecode value.  So let's just use CBloom's variant instead. This way, we don't need to keep a numcodesperlen table. 

Anyways, we really only need to transmit the codelens and corresponding symbols, and then we can then generate everything we need
to encode and decode our data.  So you have an array like :

{ 0 , 0 , 0 , 4 , 5 , 7 , 6, 0 , 12 , 5 , 0, 0, 0 ... }

where 0s represent alphabet symbols with frequency 0.  For groups of 0s, we can use run length encodings or 
some such to conserve space.  We can use the Nelson format for example.  Then to build out our codes, we order by 
codelen and then by symbol numbering within that codelen.

For example:
codeLen 4 : symbols 7, 33, 48
codeLen 5 : symbols 1, 6, 8, 40 , 44
codeLen 7 : symbols 3, 5, 22

And now we know how to generate our base and offset tables, and our symbol_ids, because the probabilities are implicit. 
Smaller codelens have lower probability, and lower symbols will be coded with lower codes within that grouping.  

