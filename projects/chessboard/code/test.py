user = input("\n\nwhat eber you awamt\n\n")

print("user before strip(): ", end ='')
print(user) 

new_user= user.strip("&")

# user "dasSO[IVHwovu]"

if user[0] == "&" or user[-1]  == "&":
    print("amper in fisrt or ;ast")

print("user after strip():  ", end ='')
print(new_user) 
