package java.lang;

interface Throwable {
}

public class Exception implements Throwable {
  private ExceptionStackItem    first;
  private String                msg;
  public Exception(String msg) {
    this.msg = msg;
    this.first = null;
  }
  public Exception() {
  }
}

