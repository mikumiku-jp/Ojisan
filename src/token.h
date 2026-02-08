#ifndef OJISAN_TOKEN_H
#define OJISAN_TOKEN_H

typedef enum {
    
    TOK_EOF,

    
    TOK_ERROR,

    
    TOK_IDENTIFIER,
    TOK_STRING,
    TOK_NUMBER,

    
    TOK_MADE_KANKEI,       
    TOK_NO_MEMBER,         
    TOK_YARIKATA,          
    TOK_SAN_KOTO,          
    TOK_SAN_KOTO_OSHIMAI,  
    TOK_ONEGAI,            
    TOK_OSHIETE_YO,        
    TOK_CHOTTO_KIITE,      
    TOK_KININARU,          
    TOK_HAJIME_OSHIMAI,    
    TOK_NI_NACCHATTA,      
    TOK_DOKIDOKI_OSHIMAI,  
    TOK_DOKIDOKI,          
    TOK_YARIKATA_OSHIMAI,  
    TOK_SOUJANAKATTARA,    
    TOK_DOCCHI_NI_SHITEMO, 
    TOK_HAJIMEMASHITE,     
    TOK_TORIYOSE,          
    TOK_SAN_WO_TSUKURU,    
    TOK_SUUJI_NI,          
    TOK_MOJI_NI,           
    TOK_KATA_WO,           
    TOK_NAGASA_WO,         
    TOK_RANDOM_CHAN,       
    TOK_MOSHIKASHITE,      
    TOK_NANCHATTE,         
    TOK_CHIGAU_KANA,       
    TOK_ONAJI_KANA,        
    TOK_MOU_II,            
    TOK_YABAKATTA,         
    TOK_WO_TSUIKA,         
    TOK_TSUGI_IKOU,        
    TOK_AIDA_WA,           
    TOK_MOU_MURI,          
    TOK_TSUBUYAKI,         
    TOK_OHHA,              
    TOK_OKKEE,             
    TOK_NAGASA_CHAN,       
    TOK_BANME_CHAN_WA,     
    TOK_BANME_CHAN,        
    TOK_CHIGAU_YO,         
    TOK_YORI_UE,           
    TOK_YORI_SHITA,        
    TOK_KOTAE,             
    TOK_DA_YO,             
    TOK_NANDA,             
    TOK_NAI_NAI,           
    TOK_MOSHIKUWA,         
    TOK_IJOU,              
    TOK_IKA,               
    TOK_BOKU_NO,           
    TOK_SHIKAMO,           
    TOK_KAKERU,            
    TOK_KANA,              
    TOK_CHAN_WA,           
    TOK_CHAN_GA,           
    TOK_CHAN_NI,           
    TOK_AMARI,             
    TOK_MAINASU,           
    TOK_MAJI,              
    TOK_CHAN,              
    TOK_HIKU,              
    TOK_WARU,              
    TOK_KARA,              
    TOK_USO,               
    TOK_TO,                
    TOK_NO,                
    TOK_COMMA,             
    TOK_ARROW,             
    TOK_LBRACKET,          
    TOK_RBRACKET,          
    TOK_LDICT,             
    TOK_RDICT,             
    TOK_LPAREN,            
    TOK_RPAREN             
} TokenType;

typedef struct {
    TokenType type;
    const char* start;
    int length;
    int line;
} Token;

void print_token(Token token);

#endif 
