
#include <iostream>

using namespace std;

int main()
{
// Bubble short

int s=6;
int count=5;
int a[6]={12,45,23,51,19,8};
for(int i=0;i<s-1;i++){
    for(int j=0;j<count;j++){
        if(a[j]>a[j+1]){
            int c=a[j];
            a[j]=a[j+1];
            a[j+1]=c;
        }
    }
    count--;
}
for(int i=0;i<6;i++){
    cout<<a[i]<<" ";
}
    return 0;
}
