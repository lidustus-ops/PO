from locust import HttpUser, task, between

class MyServerTest(HttpUser):
    wait_time = between(1, 2) # пауза між запитами

    @task
    def test_home(self):
        self.client.get("/")

    @task
    def test_page2(self):
        self.client.get("/page2.html")