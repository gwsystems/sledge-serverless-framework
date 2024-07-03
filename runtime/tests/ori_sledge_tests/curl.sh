#curl -I -w "Total time: %{time_total}s\n" http://10.10.1.1:31850/empty
#curl -w "Total time: %{time_total}s\n" http://10.10.1.1:31850/empty http://10.10.1.1:31850/empty http://10.10.1.1:31850/empty http://10.10.1.1:31850/empty
curl -w "Total time: %{time_total}s TCP Connect time: %{time_connect}s\n" -o /dev/null -s http://10.10.1.1:31850/empty http://10.10.1.1:31850/empty 


#curl -H 'Expect:' -H "Content-Type: image/bmp" --data-binary "@frog5.bmp" "10.10.1.1:31850/classify"
