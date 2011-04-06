/*
字符串全编变小写
*/
char *str_tolower(char *str, size_t size) 
{
    
    uint32_t i;
    
    for (i = 0; (i<size) || (size==0); i++) 
    {
        if (str[i] == '\0')
        {
            return str;    
        }
        if (isupper(str[i]))
        {
            str[i] = str[i] - 'A' + 'a';
        }
    }
        return str;
}