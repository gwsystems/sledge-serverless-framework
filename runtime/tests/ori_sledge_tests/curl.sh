#curl -I -w "Total time: %{time_total}s\n" http://10.10.1.1:31850/empty
curl -w "Total time: %{time_total}s\n" http://10.10.1.1:31850/empty
